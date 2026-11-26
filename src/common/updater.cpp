// Updates from the project's GitHub releases. The launcher calls StartCheck when it opens; when the latest release is
// newer than this build, its Updates row offers it and StartInstall:
//  1. downloads the release's CrimsonSkiesRecomp-<version>-win64.zip into update\ beside the exe, hashing it as it
//     comes, and refuses it when the SHA-256 differs from the one GitHub lists for the file;
//  2. unpacks it with Windows' own tar.exe (Windows 10 1803 and later);
//  3. moves each new file over the old one, renaming the old one to <name>.old first (Windows lets a running exe and
//     its loaded DLLs be renamed, not overwritten) and undoing all of it when a move fails;
//  4. Relaunch starts the new exe, whose Cleanup deletes the .old files and the download.
// The player's files (settings.toml, saves in hdd\, the game in assets\, the built game code) are not in the zip, so
// they stay; the new exe rebuilds the game code because its stamp names the exe's size and date.
#include "updater.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <bcrypt.h>
#include <winhttp.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "app/app.h"
#include "core/log.h"

#ifndef CS_VERSION
#define CS_VERSION "0.0.0"
#endif

namespace fs = std::filesystem;

namespace updater {

bool g_check = true;

namespace {

const wchar_t* kLatest = L"https://api.github.com/repos/nacholuver1/Crimson-Skies-High-Road-To-Revenge-Repo/releases/latest";

std::atomic<State> g_state{State::Idle};
std::atomic<int> g_progress{0};
std::mutex g_mutex;  // the strings below
std::string g_latest, g_message, g_url, g_sha256;

void Fail(const std::string& why) {
    LOGE("Update: %s", why.c_str());
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_message = why;
    }
    g_state = State::Failed;
}

fs::path UpdateDir() { return app::ExeDir() / "update"; }
fs::path OldList() { return UpdateDir() / "old.txt"; }

// "v1.10.2" -> {1, 10, 2}
std::vector<int> Parse(const std::string& v) {
    std::vector<int> n;
    int cur = -1;
    for (char c : v) {
        if (c >= '0' && c <= '9') cur = (cur < 0 ? 0 : cur * 10) + (c - '0');
        else if (cur >= 0) n.push_back(cur), cur = -1;
    }
    if (cur >= 0) n.push_back(cur);
    while (n.size() < 3) n.push_back(0);
    return n;
}

std::wstring Wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

// the string value of "key" at or after pos in a JSON text; empty when missing
std::string JsonString(const std::string& json, const char* key, size_t pos = 0, size_t* found = nullptr) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = json.find(k, pos);
    if (p == std::string::npos) return {};
    p = json.find(':', p + k.size());
    if (p == std::string::npos) return {};
    p = json.find('"', p);
    if (p == std::string::npos) return {};
    // up to the closing quote, decoding the escapes GitHub's names and addresses can hold (\" \\ \/ \n \t)
    std::string v;
    for (size_t i = p + 1; i < json.size(); i++) {
        char c = json[i];
        if (c == '"') {
            if (found) *found = p;
            return v;
        }
        if (c == '\\' && i + 1 < json.size()) {
            c = json[++i];
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
        }
        v += c;
    }
    return {};
}

// SHA-256 with Windows' CNG, fed as the download comes
struct Sha256 {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    Sha256() {
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0)
            BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
    }
    ~Sha256() {
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    }
    void Add(const void* p, size_t n) {
        if (hash) BCryptHashData(hash, PUCHAR(p), ULONG(n), 0);
    }
    std::string Hex() {
        unsigned char d[32];
        if (!hash || BCryptFinishHash(hash, d, sizeof d, 0) != 0) return {};
        char s[65];
        for (int i = 0; i < 32; i++) std::snprintf(s + i * 2, 3, "%02x", d[i]);
        return s;
    }
};

// an HTTPS GET; each chunk goes to sink with the total size when the server gave one. Redirects (GitHub's downloads
// go to its file servers) are followed by WinHTTP.
bool Get(const std::wstring& url, const std::function<bool(const char*, size_t, uint64_t)>& sink, std::string& err) {
    URL_COMPONENTS uc{sizeof(uc)};
    wchar_t host[256] = {}, path[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = DWORD(std::size(host));
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = DWORD(std::size(path));
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        err = "bad address";
        return false;
    }
    std::wstring agent = L"CrimsonSkiesRecomp/" + Wide(CS_VERSION);
    HINTERNET s = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) {
        err = "no internet access (WinHTTP)";
        return false;
    }
    WinHttpSetTimeouts(s, 10000, 10000, 15000, 30000);
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                    : nullptr;
    bool ok = false;
    if (r && WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(r, nullptr)) {
        DWORD status = 0, len = sizeof status;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &len, nullptr);
        wchar_t cl[32] = {};
        DWORD cll = sizeof cl;
        uint64_t total = WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, cl, &cll, nullptr) ? _wcstoui64(cl, nullptr, 10) : 0;
        if (status != 200) {
            err = "server answered " + std::to_string(status);
        } else {
            std::vector<char> buf(1 << 16);
            ok = true;
            for (;;) {
                DWORD got = 0;
                if (!WinHttpReadData(r, buf.data(), DWORD(buf.size()), &got)) {
                    err = "connection lost";
                    ok = false;
                    break;
                }
                if (got == 0) break;
                if (!sink(buf.data(), got, total)) {
                    err = "could not write the download";
                    ok = false;
                    break;
                }
            }
        }
    } else {
        err = "could not reach GitHub (offline?)";
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}

void Check() {
    std::string json, err;
    // CS_UPDATE_URL: another releases/latest address, for testing the updater against a local server
    const wchar_t* override_url = _wgetenv(L"CS_UPDATE_URL");
    if (!Get(override_url && *override_url ? override_url : kLatest, [&](const char* p, size_t n, uint64_t) { json.append(p, n); return json.size() < (8u << 20); }, err)) {
        Fail("Could not check for updates: " + err);
        return;
    }
    std::string tag = JsonString(json, "tag_name"), name = JsonString(json, "name");
    if (tag.empty()) {
        Fail("Could not check for updates: no release found");
        return;
    }
    if (!(Parse(CS_VERSION) < Parse(tag))) {
        LOGI("Update: %s is the latest (%s)", CS_VERSION, tag.c_str());
        g_state = State::UpToDate;
        return;
    }
    // the win64 zip, and its digest: listed before browser_download_url in the same asset
    size_t pos = 0, at = 0, prev = 0;
    std::string url, sha;
    for (;;) {
        std::string u = JsonString(json, "browser_download_url", pos, &at);
        if (u.empty()) break;
        if (u.size() > 10 && u.compare(u.size() - 10, 10, "-win64.zip") == 0) {
            size_t d = json.rfind("\"digest\"", at);
            if (d != std::string::npos && d > prev) {
                std::string v = JsonString(json, "digest", d);
                if (v.rfind("sha256:", 0) == 0) sha = v.substr(7);
            }
            url = u;
            break;
        }
        prev = at;
        pos = at + 1;
    }
    if (url.empty()) {
        Fail("Release " + tag + " has no Windows download yet");
        return;
    }
    std::string latest = tag[0] == 'v' || tag[0] == 'V' ? tag.substr(1) : tag;
    LOGR("Update: %s available (this is %s): %s", latest.c_str(), CS_VERSION, url.c_str());
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_latest = latest;
        g_message = name;
        g_url = url;
        g_sha256 = sha;
    }
    g_state = State::Available;
}

// runs a program hidden and waits; true when it returned 0
bool Run(std::wstring cmd) {
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

void Install() {
    std::string url, sha;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        url = g_url;
        sha = g_sha256;
    }
    std::error_code ec;
    fs::path dir = UpdateDir(), zip = dir / "download.zip", unpacked = dir / "unpacked";
    fs::remove_all(unpacked, ec);
    fs::create_directories(dir, ec);

    // 1. download, hashed as it comes
    g_progress = 0;
    g_state = State::Downloading;
    {
        std::ofstream out(zip, std::ios::binary | std::ios::trunc);
        Sha256 hash;
        uint64_t got = 0;
        std::string err;
        bool ok = out && Get(Wide(url), [&](const char* p, size_t n, uint64_t total) {
            out.write(p, std::streamsize(n));
            hash.Add(p, n);
            got += n;
            if (total) g_progress = int(got * 100 / total);
            return bool(out);
        }, err);
        out.close();
        if (!ok) {
            Fail("Download failed: " + err);
            return;
        }
        std::string have = hash.Hex();
        if (sha.size() != 64 || have != sha) {
            fs::remove(zip, ec);
            Fail("The download is damaged (checksum differs).");
            return;
        }
        LOGI("Update: downloaded %llu bytes, sha256 %s (matches)", (unsigned long long)got, have.c_str());
    }

    // 2. unpack with Windows' tar.exe
    g_state = State::Installing;
    wchar_t sys[MAX_PATH] = L"C:\\Windows\\System32";
    GetSystemDirectoryW(sys, MAX_PATH);
    fs::create_directories(unpacked, ec);
    std::wstring cmd = L"\"" + (fs::path(sys) / "tar.exe").wstring() + L"\" -xf \"" + zip.wstring() + L"\" -C \"" +
                       unpacked.wstring() + L"\"";
    if (!Run(cmd)) {
        Fail("Could not unpack the download (needs Windows 10 1803 or later)");
        return;
    }
    // the zip holds one folder, CrimsonSkiesRecomp-<version>-win64, with the exe in it
    fs::path root;
    if (fs::exists(unpacked / "CrimsonSkies.exe", ec)) root = unpacked;
    for (auto& e : fs::directory_iterator(unpacked, ec))
        if (root.empty() && e.is_directory() && fs::exists(e.path() / "CrimsonSkies.exe", ec)) root = e.path();
    if (root.empty()) {
        Fail("The download has no CrimsonSkies.exe");
        return;
    }

    // 3. each new file over the old one, the old one kept as .old until all moved
    fs::path exe = app::ExeDir();
    std::vector<std::pair<fs::path, bool>> done;  // target, whether an old file was set aside
    auto undo = [&] {
        for (auto it = done.rbegin(); it != done.rend(); ++it) {
            fs::path old = it->first;
            old += L".old";
            if (it->second) MoveFileExW(old.c_str(), it->first.c_str(), MOVEFILE_REPLACE_EXISTING);
            else DeleteFileW(it->first.c_str());
        }
    };
    std::vector<fs::path> files;
    for (auto& e : fs::recursive_directory_iterator(root, ec))
        if (e.is_regular_file()) files.push_back(e.path());
    if (ec) {  // a partial list would leave a mix of versions
        Fail("Could not read the unpacked files");
        return;
    }
    for (const fs::path& src : files) {
        fs::path rel = fs::relative(src, root, ec), target = exe / rel;
        // never the player's files, should a release ever carry them
        bool player = rel.filename() == L"settings.toml";
        for (auto& part : rel) player |= part == L"hdd" || part == L"work";
        if (player) continue;
        fs::create_directories(target.parent_path(), ec);
        fs::path old = target;
        old += L".old";
        bool had = fs::exists(target, ec);
        if (had && !MoveFileExW(target.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            undo();
            std::u8string u = rel.u8string();
            Fail("Could not replace " + std::string(u.begin(), u.end()) + " (is it open?)");
            return;
        }
        if (!MoveFileExW(src.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            if (had) MoveFileExW(old.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
            undo();
            Fail("Could not write the new files (is the folder read-only?)");
            return;
        }
        done.push_back({target, had});
    }
    {
        std::ofstream list(OldList(), std::ios::trunc);
        for (auto& [target, had] : done)
            if (had) {
                fs::path old = fs::relative(target, exe, ec);  // relative: the folder may be moved before the next start
                old += L".old";
                std::u8string u = old.u8string();
                list << std::string(u.begin(), u.end()) << "\n";
            }
    }
    fs::remove(zip, ec);
    fs::remove_all(unpacked, ec);
    LOGR("Update: installed %s (%zu files)", Latest().c_str(), done.size());
    g_state = State::Ready;
}

}  // namespace

bool Download(const std::string& url, const fs::path& to, const std::string& sha256,
              const std::function<void(uint64_t, uint64_t)>& progress, std::string& err) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    fs::path part = to;
    part += ".part";
    Sha256 hash;
    bool ok;
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        uint64_t got = 0;
        ok = out && Get(Wide(url), [&](const char* p, size_t n, uint64_t total) {
            out.write(p, std::streamsize(n));
            hash.Add(p, n);
            got += n;
            if (progress) progress(got, total);
            return bool(out);
        }, err);
    }
    if (ok && hash.Hex() != sha256) {
        err = "the download is damaged (checksum differs)";
        ok = false;
    }
    if (ok) fs::rename(part, to, ec);
    if (!ok || ec) {
        if (ok) err = "could not save " + to.filename().string();
        fs::remove(part, ec);
        return false;
    }
    return true;
}

bool Unpack(const fs::path& zip, const fs::path& dir, const std::wstring& only) {
    wchar_t sys[MAX_PATH] = L"C:\\Windows\\System32";
    GetSystemDirectoryW(sys, MAX_PATH);
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::wstring cmd = L"\"" + (fs::path(sys) / "tar.exe").wstring() + L"\" -xf \"" + zip.wstring() + L"\" -C \"" +
                       dir.wstring() + L"\"";
    if (!only.empty()) cmd += L" \"" + only + L"\"";
    return Run(cmd);
}

const char* Version() { return CS_VERSION; }

void Cleanup() {
    std::error_code ec;
    fs::path dir = UpdateDir();
    if (!fs::exists(dir, ec)) return;
    std::vector<std::string> left;
    std::ifstream list(OldList());
    std::string line;
    // the previous version may still be closing: tries for up to 3 seconds in all, then the next start goes on
    ULONGLONG deadline = GetTickCount64() + 3000;
    while (std::getline(list, line)) {
        if (line.empty()) continue;
        fs::path old = app::ExeDir() / fs::path(std::u8string(line.begin(), line.end()));
        for (;;) {
            if (DeleteFileW(old.c_str())) break;
            DWORD e = GetLastError();
            if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) break;
            if (GetTickCount64() >= deadline) {
                left.push_back(line);
                break;
            }
            Sleep(100);
        }
    }
    list.close();
    if (left.empty()) {
        fs::remove_all(dir, ec);
    } else {
        fs::remove(dir / "download.zip", ec);
        fs::remove_all(dir / "unpacked", ec);
        std::ofstream out(OldList(), std::ios::trunc);
        for (auto& l : left) out << l << "\n";
    }
}

void StartCheck() {
    if (std::wcsstr(GetCommandLineW(), L"--no-update-check")) return;
    // started by RecompLauncher, which installs the releases itself: two updaters on one folder would fight
    if (const char* rl = std::getenv("RECOMPLAUNCHER"); rl && std::strcmp(rl, "1") == 0) return;
    State s = g_state;
    if (s == State::Checking || s == State::Downloading || s == State::Installing || s == State::Ready) return;
    g_state = State::Checking;
    std::thread(Check).detach();
}

void StartInstall() {
    if (g_state != State::Available && g_state != State::Failed) return;
    if (g_state == State::Failed) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_url.empty()) return;  // the check failed: nothing to install
    }
    g_progress = 0;
    g_state = State::Downloading;
    std::thread(Install).detach();
}

State GetState() { return g_state; }
int Progress() { return g_progress; }

std::string Latest() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_latest;
}

std::string Message() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_message;
}

bool Relaunch() {
    fs::path exe = app::ExeDir() / "CrimsonSkies.exe";
    std::wstring cmd = GetCommandLineW();
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    std::wstring dir = app::ExeDir().wstring();
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

}  // namespace updater
