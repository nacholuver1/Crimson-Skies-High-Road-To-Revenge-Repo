#include "ai_movies.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <cctype>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "gpu/texture.h"
#include "gpu/texturedumper.h"
#include "kernel/disc.h"
#include "updater.h"

namespace fs = std::filesystem;

namespace ai_movies {
namespace {

// the tools, pinned: a changed file is refused
struct Tool {
    const char* url;
    const char* sha256;
    const char* folder;  // under tools\ beside the exe
    const wchar_t* member;  // what to unpack, empty for everything
    const char* exe;     // the program, from the folder
    const char* what;
};
const Tool kUpscaler = {
    "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-windows.zip",
    "abc02804e17982a3be33675e4d471e91ea374e65b70167abc09e31acb412802d", "realesrgan", L"",
    "realesrgan-ncnn-vulkan.exe", "Real-ESRGAN (45 MB)"};
const Tool kFfmpeg = {
    "https://github.com/GyanD/codexffmpeg/releases/download/9.1/ffmpeg-9.1-essentials_build.zip",
    "b9bbbb3acece000cbeec994f856649354e52c773807841e78908374b24d658fd", "ffmpeg",
    L"ffmpeg-9.1-essentials_build/bin/ffmpeg.exe", "ffmpeg-9.1-essentials_build/bin/ffmpeg.exe", "FFmpeg (119 MB)"};

// what the files in movies\ai\ were made with; another value makes them again
constexpr const char* kRecipe = "1: realesr-animevideov3 x2, bwdif on interlaced movies, h264_mf";
constexpr const char* kModel = "realesr-animevideov3";
constexpr int kScale = 2;
constexpr uint32_t kChunk = 300;  // frames upscaled at a time: bounds the disk the step needs

// movies with interlacing combs in their frames (FFmpeg idet over all 60); the rest are progressive
const std::set<std::string> kInterlaced = {"attractmode",             "fullendcredits",
                                           "map_transition_ax_to_ch", "map_transition_ch_to_lc",
                                           "map_transition_lc_to_ch", "map_transition_sh_to_ax",
                                           "ss_farewell"};

const char* const kFolders[] = {"bink1", "bink2"};

fs::path Out() { return app::ExeDir() / "movies" / "ai"; }
fs::path Work() { return app::ExeDir() / "movies" / "work"; }
fs::path Tools() { return app::ExeDir() / "tools"; }

std::string Lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// a line for whoever started the step (stdout)
void Print(const char* line) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out && out != INVALID_HANDLE_VALUE) {
        DWORD n;
        WriteFile(out, line, DWORD(std::strlen(line)), &n, nullptr);
        WriteFile(out, "\n", 1, &n, nullptr);
    }
}

// a message: printed and logged
void Say(const char* fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    LOGR("ai movies: %s", line);
    Print(line);
}

// the share of the whole run the current part fills: progress lines are scaled into it
double g_base = 0, g_span = 1;

// a progress line; the log gets each new text once, not every step of it
void SayProgress(double f, const char* fmt, ...) {
    f = g_base + g_span * std::clamp(f, 0.0, 1.0);
    char text[400], line[440];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    static std::string logged;
    std::string what = text;
    if (what.rfind("Downloading", 0) == 0) what = what.substr(0, what.find(':'));
    if (what != logged) {
        logged = what;
        LOGR("ai movies: %s", text);
    }
    std::snprintf(line, sizeof line, "progress %.3f %s", std::clamp(f, 0.0, 1.0), text);
    Print(line);
}

// one of the game's movies, from the disc image or the game folder
struct Movie {
    std::string name;  // file name without .bik, as the disc has it
    uint32_t frames = 0, fps_num = 30000, fps_den = 1001;
    const kernel::Disc::Entry* entry = nullptr;  // on the disc
    fs::path file;                                // in a folder
};

struct Game {
    std::unique_ptr<kernel::Disc> disc;
    std::vector<Movie> movies;
    std::string error;
};

// the Bink header: frames at +8, frame rate as dividend +0x1C / divider +0x20
void ReadHeader(const uint8_t* h, Movie& m) {
    auto u32 = [&](int at) { return uint32_t(h[at]) | uint32_t(h[at + 1]) << 8 | uint32_t(h[at + 2]) << 16 | uint32_t(h[at + 3]) << 24; };
    m.frames = u32(8);
    if (u32(0x1C) && u32(0x20)) m.fps_num = u32(0x1C), m.fps_den = u32(0x20);
}

// the game as chosen: a disc image, or the folder of the extracted game (picked as the folder or its default.xbe)
fs::path GamePath() {
    fs::path path = app::Get().game_path.empty()
                        ? app::DefaultGamePath()
                        : fs::path(reinterpret_cast<const char8_t*>(app::Get().game_path.c_str()));
    if (Lower(path.extension().string()) == ".xbe") path = path.parent_path();
    return path;
}

Game FindMovies() {
    Game g;
    fs::path path = GamePath();
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec)) {
        g.error = "The game was not found: choose your disc image or game folder first.";
        return g;
    }
    if (fs::is_regular_file(path, ec)) {
        g.disc = std::make_unique<kernel::Disc>();
        if (!g.disc->Open(path, g.error)) return g;
        for (const char* folder : kFolders) {
            const kernel::Disc::Entry* dir = g.disc->Find(folder);
            if (!dir) continue;
            for (const auto& e : dir->children) {
                if (e.dir || e.name.size() < 5 || Lower(e.name.substr(e.name.size() - 4)) != ".bik") continue;
                Movie m;
                m.name = e.name.substr(0, e.name.size() - 4);
                m.entry = &e;
                uint8_t h[0x24] = {};
                g.disc->Read(e, 0, h, sizeof h);
                ReadHeader(h, m);
                g.movies.push_back(m);
            }
        }
    } else {
        for (const char* folder : kFolders) {
            for (auto& e : fs::directory_iterator(path / folder, ec)) {
                if (!e.is_regular_file() || Lower(e.path().extension().string()) != ".bik") continue;
                Movie m;
                m.name = e.path().stem().string();
                m.file = e.path();
                uint8_t h[0x24] = {};
                std::ifstream(m.file, std::ios::binary).read(reinterpret_cast<char*>(h), sizeof h);
                ReadHeader(h, m);
                g.movies.push_back(m);
            }
        }
    }
    if (g.movies.empty()) g.error = "No movies were found in the game (bink1 and bink2 folders).";
    return g;
}

bool RecipeMatches() {
    std::ifstream f(Out() / "recipe.txt");
    std::string line;
    return std::getline(f, line) && line == kRecipe;
}

bool Made(const Movie& m) {
    std::error_code ec;
    return fs::exists(Out() / (m.name + ".mp4"), ec);
}

// a program run hidden at low priority, its output thrown away; killed with this process (job object)
bool RunTool(const fs::path& exe, const std::wstring& args, const fs::path& cwd) {
    static HANDLE job = [] {
        HANDLE j = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (j) SetInformationJobObject(j, JobObjectExtendedLimitInformation, &li, sizeof li);
        return j;
    }();
    std::wstring cmd = L"\"" + exe.wstring() + L"\" " + args;
    STARTUPINFOW si{sizeof si};
    si.dwFlags = STARTF_USESTDHANDLES;
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};  // inherited by the tool
    HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    si.hStdInput = nullptr;
    si.hStdOutput = nul;
    si.hStdError = nul;
    PROCESS_INFORMATION pi{};
    bool ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS | CREATE_SUSPENDED, nullptr,
                             cwd.wstring().c_str(), &si, &pi);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) return false;
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

std::wstring Wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), w.data(), n);
    return w;
}

std::wstring Q(const fs::path& p) { return L"\"" + p.wstring() + L"\""; }

// the tool's program, downloaded and unpacked first when missing; empty with the reason said
fs::path GetTool(const Tool& t, double at, double span) {
    fs::path dir = Tools() / t.folder, exe = dir / t.exe;
    std::error_code ec;
    if (fs::exists(exe, ec)) return exe;
    fs::path zip = Tools() / (std::string(t.folder) + ".zip");
    std::string err;
    int last = -1;
    bool ok = updater::Download(t.url, zip, t.sha256, [&](uint64_t got, uint64_t total) {
        int pct = total ? int(got * 100 / total) : 0;
        if (pct / 5 != last / 5) {
            last = pct;
            SayProgress(at + span * pct / 100.0, "Downloading %s: %d%%", t.what, pct);
        }
    }, err);
    if (!ok) {
        Say("Could not download %s: %s.", t.what, err.c_str());
        return {};
    }
    ok = updater::Unpack(zip, dir, t.member);
    fs::remove(zip, ec);
    if (!ok || !fs::exists(exe, ec)) {
        Say("Could not unpack %s.", t.what);
        return {};
    }
    return exe;
}

// the movie's Bink file where FFmpeg can read it: the folder's own, or copied off the disc into the work folder
bool BinkFile(const Game& g, const Movie& m, fs::path& out) {
    if (!m.entry) {
        out = m.file;
        return true;
    }
    out = Work() / (m.name + ".bik");
    std::ofstream f(out, std::ios::binary | std::ios::trunc);
    std::vector<char> buf(1 << 20);
    for (uint64_t pos = 0; pos < m.entry->size;) {
        uint32_t n = g.disc->Read(*m.entry, pos, buf.data(), uint32_t(buf.size()));
        if (!n) break;
        f.write(buf.data(), n);
        pos += n;
    }
    return bool(f) && f.tellp() == std::streamoff(m.entry->size);
}

size_t CountFiles(const fs::path& dir) {
    std::error_code ec;
    size_t n = 0;
    for (auto& e : fs::directory_iterator(dir, ec)) n += e.is_regular_file();
    return n;
}

// one movie: frames out of the Bink file, upscaled a chunk at a time into H.264 parts, the parts joined
bool MakeMovie(const Game& g, const Movie& m, const fs::path& ffmpeg, const fs::path& upscaler,
               const std::function<void(uint32_t)>& done_frames) {
    std::error_code ec;
    fs::remove_all(Work(), ec);
    fs::path in = Work() / "in", chunk = Work() / "chunk", up = Work() / "up";
    for (auto& d : {in, chunk, up}) fs::create_directories(d, ec);
    fs::path bik;
    if (!BinkFile(g, m, bik)) {
        Say("Could not read %s.bik from the game.", m.name.c_str());
        return false;
    }
    std::wstring filter = kInterlaced.count(Lower(m.name)) ? L"idet,bwdif=mode=send_frame:parity=auto:deint=interlaced"
                                                            : L"null";
    if (!RunTool(ffmpeg, L"-v error -i " + Q(bik) + L" -an -fps_mode passthrough -vf " + filter + L" " +
                             Q(in / "%06d.png"), Work())) {
        Say("FFmpeg could not decode %s.bik.", m.name.c_str());
        return false;
    }
    uint32_t frames = uint32_t(CountFiles(in));
    if (!frames || (m.frames && frames != m.frames)) {
        Say("%s.bik gave %u frames, its header says %u.", m.name.c_str(), frames, m.frames);
        return false;
    }
    std::wstring rate = std::to_wstring(m.fps_num) + L"/" + std::to_wstring(m.fps_den);
    std::ofstream list(Work() / "parts.txt", std::ios::trunc);
    for (uint32_t first = 1, part = 0; first <= frames; first += kChunk, part++) {
        uint32_t last = std::min(frames, first + kChunk - 1);
        fs::remove_all(chunk, ec);
        fs::remove_all(up, ec);
        fs::create_directories(chunk, ec);
        fs::create_directories(up, ec);
        for (uint32_t i = first; i <= last; i++) {
            wchar_t n[16];
            std::swprintf(n, 16, L"%06u.png", i);
            fs::rename(in / n, chunk / n, ec);
            if (ec) return false;
        }
        if (!RunTool(upscaler, L"-i " + Q(chunk) + L" -o " + Q(up) + L" -n " + Wide(kModel) + L" -s " +
                                   std::to_wstring(kScale) + L" -f png",
                     upscaler.parent_path()) ||
            CountFiles(up) != last - first + 1) {
            Say("Real-ESRGAN failed on %s (a Vulkan graphics driver is needed).", m.name.c_str());
            return false;
        }
        wchar_t name[32];
        std::swprintf(name, 32, L"part%04u.mp4", part);
        if (!RunTool(ffmpeg, L"-v error -y -framerate " + rate + L" -start_number " + std::to_wstring(first) + L" -i " +
                                 Q(up / "%06d.png") + L" -c:v h264_mf -b:v 12M -rate_control quality -quality 80 "
                                 L"-pix_fmt nv12 " + Q(Work() / name),
                     Work())) {
            Say("FFmpeg could not encode %s (Windows' H.264 encoder, Media Foundation, is needed).", m.name.c_str());
            return false;
        }
        list << "file '" << fs::path(name).string() << "'\n";
        done_frames(last - first + 1);
    }
    list.close();
    fs::path joined = Work() / "joined.mp4";
    if (!RunTool(ffmpeg, L"-v error -y -f concat -safe 0 -i parts.txt -c copy " + Q(joined), Work())) {
        Say("FFmpeg could not join the parts of %s.", m.name.c_str());
        return false;
    }
    fs::create_directories(Out(), ec);
    fs::rename(joined, Out() / (m.name + ".mp4"), ec);
    if (ec) {
        Say("Could not write %s.mp4 into movies\\ai.", m.name.c_str());
        return false;
    }
    fs::remove_all(Work(), ec);
    return true;
}

}  // namespace

bool Wanted() {
    const wchar_t* cmd = GetCommandLineW();
    return std::wcsstr(cmd, L"--ai-movies") || std::wcsstr(cmd, L"--ai-textures") || std::wcsstr(cmd, L"--prepare");
}

Status Check() {
    Status s;
    Game g = FindMovies();
    s.total = int(g.movies.size());
    if (!RecipeMatches()) return s;
    for (const Movie& m : g.movies) s.made += Made(m);
    return s;
}

namespace {

// AI textures: every texture in the game's level bundles (the root *.bin files), upscaled into textures\ai\ under the
// key the game gives it, so the renderer's texture pack draws it (sdk gpu/texturedumper).

constexpr const char* kTexRecipe = "1: realesrgan-x4plus x4 then halved, alpha resized, ui_/hud_/data textures left out";
constexpr uint32_t kTexChunk = 64;  // images per upscaler run: progress steps

fs::path TexOut() { return app::ExeDir() / "textures" / "ai"; }
// one per run, so two runs at once (our launcher and RecompLauncher's setup, say) never clear each other's
fs::path TexWork() {
    return app::ExeDir() / "textures" / ("ai-work-" + std::to_string(GetCurrentProcessId()));
}

// a texture entry in a bundle: <u32 2><u32 name bytes><name .tga/.dds NUL><u32 total><u16 w><u16 h><u16 mips>
// <u8><u32 game format><u32 level 0 bytes><levels...>
struct GameTexture {
    std::string name;
    gpu::TextureDesc desc;
    std::vector<uint8_t> level0;
    uint64_t key = 0;
};

// the game's format codes as the console's (the keys of 775 dumps match): DXT1, DXT5, R5G6B5, X8R8G8B8, A8R8G8B8
uint32_t XboxFormat(uint32_t game) {
    switch (game) {
        case 1: return 0x0C;
        case 5: return 0x0F;
        case 14: return 0x05;
        case 15: return 0x07;
        case 16: return 0x06;
        default: return 0;  // the palette (P8) textures are left as they are
    }
}

bool NameChar(uint8_t c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '!' || c == '@' || c == ' ' || c == '.';
}

void ParseBundle(const std::vector<uint8_t>& d, std::vector<GameTexture>& out, std::set<uint64_t>& seen) {
    auto u16 = [&](size_t at) { return uint32_t(d[at]) | uint32_t(d[at + 1]) << 8; };
    auto u32 = [&](size_t at) { return u16(at) | u16(at + 2) << 16; };
    for (size_t i = 0; i + 8 < d.size(); i++) {
        if (d[i] != 2 || d[i + 1] || d[i + 2] || d[i + 3] || d[i + 5] || d[i + 6] || d[i + 7]) continue;
        size_t n = d[i + 4], name = i + 8, head = name + n;
        if (n < 6 || n > 0x50 || head + 19 > d.size() || d[head - 1] != 0) continue;
        bool ok = true;
        for (size_t c = name; c + 1 < head && ok; c++) ok = NameChar(d[c]);
        if (!ok) continue;
        std::string nm(reinterpret_cast<const char*>(&d[name]), n - 1);
        std::string ext = Lower(nm.substr(nm.size() - 4));
        if (ext != ".tga" && ext != ".dds") continue;
        uint32_t w = u16(head + 4), h = u16(head + 6), fmt = u32(head + 11), size = u32(head + 15);
        gpu::TextureDesc desc;
        desc.format = XboxFormat(fmt);
        desc.width = w;
        desc.height = h;
        size_t data = head + 19;
        if (!desc.format || !w || !h || w > 4096 || h > 4096 || (w & (w - 1)) || (h & (h - 1)) ||
            gpu::LevelSize(desc, 0) != size || data + size > d.size())
            continue;
        GameTexture t;
        t.name = nm;
        t.desc = desc;
        t.key = gpu::texturedumper::KeyOf(desc, &d[data]);
        i = data + size - 1;
        if (!seen.insert(t.key).second) continue;
        t.level0.assign(d.begin() + data, d.begin() + data + size);
        out.push_back(std::move(t));
    }
}

// menus, the HUD and data textures stay the game's: AI upscaling bends letters and changes data
const char* SkipByName(const std::string& name) {
    std::string n = Lower(name);
    if (n.rfind("ui_", 0) == 0 || n.rfind("hud_", 0) == 0) return "menu or HUD";
    if (n.rfind("water!", 0) == 0 || n.find("mask") != std::string::npos) return "data";
    if (n.rfind("esrb", 0) == 0) return "menu or HUD";
    if (n.size() > 7 && n.compare(n.size() - 7, 3, "_lm") == 0) return "light map";
    return nullptr;
}

// what the decoded picture shows: flat colour or a normal map (bluish, red and green around the middle)
const char* SkipByLook(const std::vector<uint8_t>& bgra) {
    size_t n = bgra.size() / 4;
    double sum[3] = {}, sq[3] = {};
    for (size_t i = 0; i < n; i++)
        for (int c = 0; c < 3; c++) {
            double v = bgra[i * 4 + c];
            sum[c] += v;
            sq[c] += v * v;
        }
    double b = sum[0] / n, g = sum[1] / n, r = sum[2] / n, spread = 0;
    for (int c = 0; c < 3; c++) spread = std::max(spread, sq[c] / n - (sum[c] / n) * (sum[c] / n));
    if (spread < 9) return "flat";
    if (b > 190 && r > 90 && r < 170 && g > 90 && g < 170) return "normal map";
    return nullptr;
}

std::vector<GameTexture> FindTextures(std::string& error) {
    std::vector<GameTexture> out;
    std::set<uint64_t> seen;
    fs::path path = GamePath();
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec)) {
        error = "The game was not found: choose your disc image or game folder first.";
        return out;
    }
    std::vector<uint8_t> data;
    if (fs::is_regular_file(path, ec)) {
        kernel::Disc disc;
        if (!disc.Open(path, error)) return out;
        const kernel::Disc::Entry* root = disc.Find("");
        for (const auto& e : root ? root->children : std::vector<kernel::Disc::Entry>{}) {
            if (e.dir || Lower(fs::path(e.name).extension().string()) != ".bin") continue;
            // a bundle that does not read stops the run: a pack missing its textures must not look finished
            if (!disc.Load(e.name, data) || data.size() != e.size) {
                error = "Could not read " + e.name + " from the disc image.";
                return {};
            }
            ParseBundle(data, out, seen);
        }
    } else {
        fs::directory_iterator it(path, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            const fs::path& p = it->path();
            if (!it->is_regular_file() || Lower(p.extension().string()) != ".bin") continue;
            std::ifstream f(p, std::ios::binary);
            data.assign(std::istreambuf_iterator<char>(f), {});
            std::error_code size_ec;
            if ((!f.good() && !f.eof()) || data.size() != fs::file_size(p, size_ec) || size_ec) {
                error = "Could not read " + p.filename().string() + " from the game folder.";
                return {};
            }
            ParseBundle(data, out, seen);
        }
        if (ec) {
            error = "Could not list the game folder.";
            return {};
        }
    }
    if (out.empty()) error = "No textures were found in the game's level files.";
    return out;
}

// the upscaled colour halved to 2x, with the original's alpha resized (cut-outs stay sharp-edged)
bool Finish(const GameTexture& t, const std::vector<uint8_t>& original, const fs::path& upscaled) {
    namespace td = gpu::texturedumper;
    std::vector<uint8_t> big, out, alpha;
    uint32_t w4 = 0, h4 = 0, w2 = t.desc.width * 2, h2 = t.desc.height * 2;
    if (!td::ReadBgra(upscaled, big, w4, h4) || !td::ScaleBgra(big, w4, h4, out, w2, h2)) return false;
    bool opaque = true, binary = true;
    for (size_t i = 3; i < original.size(); i += 4) {
        opaque &= original[i] == 255;
        binary &= original[i] == 0 || original[i] == 255;
    }
    if (opaque) {
        for (size_t i = 3; i < out.size(); i += 4) out[i] = 255;
    } else {
        if (!td::ScaleBgra(original, t.desc.width, t.desc.height, alpha, w2, h2)) return false;
        for (size_t i = 3; i < out.size(); i += 4)
            out[i] = binary ? (alpha[i] >= 128 ? 255 : 0) : alpha[i];
    }
    return td::WriteBgra(TexOut() / td::NameOf(t.key, t.desc), out, w2, h2);
}

bool TexturesMade() {
    std::ifstream f(TexOut() / "done.txt");
    std::string line;
    return std::getline(f, line) && line == kTexRecipe;
}

int MakeTextures() {
    namespace td = gpu::texturedumper;
    SayProgress(0, "Reading the game's textures");
    if (TexturesMade()) {
        SayProgress(1, "Already made: the AI textures");
        return 0;
    }
    std::error_code gone;
    if (fs::exists(TexOut() / "done.txt", gone)) fs::remove_all(TexOut(), gone);  // made by another recipe: again
    std::string error;
    std::vector<GameTexture> all = FindTextures(error);
    if (!error.empty()) {
        Say("%s", error.c_str());
        return 1;
    }
    std::error_code ec;
    fs::space_info sp = fs::space(app::ExeDir(), ec);
    if (!ec && sp.available < (2ull << 30)) {
        Say("Making the AI textures needs about 2 GB of free disk space beside the game (%.1f GB free).",
            double(sp.available) / (1 << 30));
        return 1;
    }
    fs::path upscaler = GetTool(kUpscaler, 0.0, 0.05);
    if (upscaler.empty()) return 1;
    // which to upscale; those already in textures\ai\ from a stopped run are kept
    fs::create_directories(TexOut(), ec);
    std::vector<const GameTexture*> todo;
    std::map<std::string, int> skipped;
    for (const GameTexture& t : all) {
        uint32_t w = t.desc.width, h = t.desc.height;
        const char* why = SkipByName(t.name);
        if (!why && std::max(w, h) <= 32) why = "tiny";
        if (!why && uint64_t(w) * h >= 1024ull * 1024) why = "already large";
        if (why) {
            skipped[why]++;
            continue;
        }
        if (!fs::exists(TexOut() / td::NameOf(t.key, t.desc), ec)) todo.push_back(&t);
    }
    size_t done = 0, made = 0, failed = 0;
    for (size_t first = 0; first < todo.size(); first += kTexChunk) {
        size_t last = std::min(todo.size(), first + kTexChunk);
        fs::path in = TexWork() / "in", up = TexWork() / "up";
        // a clean work folder each chunk, or no run: stale images could pass for this chunk's
        bool clean = (fs::remove_all(TexWork(), ec), !ec) && (fs::create_directories(in, ec), !ec) &&
                     (fs::create_directories(up, ec), !ec);
        if (!clean) {
            Say("Could not prepare the work folder %s. The textures made so far are kept.",
                TexWork().string().c_str());
            return 1;
        }
        std::vector<std::pair<const GameTexture*, std::vector<uint8_t>>> chunk;
        for (size_t i = first; i < last; i++) {
            const GameTexture& t = *todo[i];
            std::vector<uint8_t> bgra;
            // every format XboxFormat lets through decodes: a failure here is WIC's, so the next run tries again
            if (!td::DecodeBgra(t.desc, t.level0.data(), bgra)) {
                failed++;
                continue;
            }
            if (const char* why = SkipByLook(bgra)) {
                skipped[why]++;
                continue;
            }
            std::vector<uint8_t> opaque = bgra;  // the upscaler sees colour only
            for (size_t p = 3; p < opaque.size(); p += 4) opaque[p] = 255;
            if (td::WriteBgra(in / td::NameOf(t.key, t.desc), opaque, t.desc.width, t.desc.height))
                chunk.emplace_back(&t, std::move(bgra));
            else
                failed++;
        }
        SayProgress(0.05 + 0.95 * double(done) / double(std::max<size_t>(todo.size(), 1)),
                    "Upscaling textures: %zu of %zu", done, todo.size());
        if (!chunk.empty() &&
            !RunTool(upscaler, L"-i " + Q(in) + L" -o " + Q(up) + L" -n realesrgan-x4plus -s 4 -f png",
                     upscaler.parent_path())) {
            fs::remove_all(TexWork(), ec);
            Say("Real-ESRGAN failed on the textures (a Vulkan graphics driver is needed). The ones made so far are kept.");
            return 1;
        }
        for (auto& [t, bgra] : chunk) {
            if (Finish(*t, bgra, up / td::NameOf(t->key, t->desc))) made++;
            else failed++;
        }
        done = last;
    }
    fs::remove_all(TexWork(), ec);
    std::string left;
    for (auto& [why, n] : skipped) left += (left.empty() ? "" : ", ") + std::to_string(n) + " " + why;
    LOGR("ai textures: %zu made now, %zu failed, %zu found; left as they are: %s", made, failed, all.size(),
         left.c_str());
    if (failed) {
        Say("%zu textures could not be written (disk full?). The ones made are kept; the next run makes the rest.",
            failed);
        return 1;
    }
    std::ofstream(TexOut() / "done.txt") << kTexRecipe << "\n";
    size_t pack = 0;
    for (auto& e : fs::directory_iterator(TexOut(), ec)) pack += Lower(e.path().extension().string()) == ".png";
    SayProgress(1, "AI textures made: %zu textures upscaled", pack);
    return 0;
}

int MakeMovies() {
    SayProgress(0, "Looking for the game's movies");
    Game g = FindMovies();
    if (!g.error.empty()) {
        Say("%s", g.error.c_str());
        return 1;
    }
    std::error_code ec;
    // made with another recipe: all again
    if (!RecipeMatches()) {
        fs::remove_all(Out(), ec);
        fs::create_directories(Out(), ec);
        std::ofstream(Out() / "recipe.txt") << kRecipe << "\n";
    }
    std::vector<const Movie*> todo;
    uint64_t total = 0;
    for (const Movie& m : g.movies)
        if (!Made(m)) todo.push_back(&m), total += std::max(m.frames, 1u);
    if (todo.empty()) {
        SayProgress(1, "Already made: all %zu cutscenes", g.movies.size());
        return 0;
    }
    fs::space_info sp = fs::space(app::ExeDir(), ec);
    if (!ec && sp.available < (8ull << 30)) {
        Say("Making the cutscenes needs about 8 GB of free disk space beside the game (%.1f GB free).",
            double(sp.available) / (1 << 30));
        return 1;
    }
    fs::path upscaler = GetTool(kUpscaler, 0.0, 0.01);
    if (upscaler.empty()) return 1;
    fs::path ffmpeg = GetTool(kFfmpeg, 0.01, 0.03);
    if (ffmpeg.empty()) return 1;
    uint64_t done = 0;
    for (size_t i = 0; i < todo.size(); i++) {
        const Movie& m = *todo[i];
        auto report = [&](uint32_t frames) {
            done += frames;
            SayProgress(0.04 + 0.96 * double(done) / double(total), "Upscaling cutscene %zu of %zu: %s", i + 1,
                     todo.size(), m.name.c_str());
        };
        SayProgress(0.04 + 0.96 * double(done) / double(total), "Upscaling cutscene %zu of %zu: %s", i + 1, todo.size(),
                 m.name.c_str());
        if (!MakeMovie(g, m, ffmpeg, upscaler, report)) {
            fs::remove_all(Work(), ec);
            Say("Stopped at %s; the cutscenes made so far are kept, and the next run carries on.", m.name.c_str());
            return 1;
        }
    }
    SayProgress(1, "All %zu cutscenes are made", g.movies.size());
    return 0;
}

}  // namespace

int Run() {
    // the log also writes to stderr: the step's output is only its own lines (the log file keeps them all)
    std::freopen("NUL", "w", stderr);
    const wchar_t* cmd = GetCommandLineW();
    bool movies = std::wcsstr(cmd, L"--ai-movies"), textures = std::wcsstr(cmd, L"--ai-textures");
    // RecompLauncher's setup step without a tick: the game builds its code at its first start
    if (!movies && !textures) {
        SayProgress(1, "Nothing to prepare");
        return 0;
    }
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);  // WIC, for the textures' images
    int rc = 0;
    // the textures take minutes, the cutscenes most of an hour: the bar is split by time
    if (textures) {
        g_base = 0, g_span = movies ? 0.1 : 1.0;
        rc = MakeTextures();
    }
    if (movies && rc == 0) {
        g_base = textures ? 0.1 : 0, g_span = textures ? 0.9 : 1.0;
        rc = MakeMovies();
    }
    if (SUCCEEDED(co)) CoUninitialize();
    return rc;
}

namespace {

std::mutex g_mu;
HANDLE g_child = nullptr;
std::atomic<bool> g_running{false}, g_failed{false};
std::atomic<double> g_progress{0};
std::atomic<Job> g_job{Job::Movies};  // the run started last
std::string g_message;

void SetMessage(const std::string& m) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_message = m;
}

// the child's lines: "progress <f> <text>" moves the bar, any other line is a message to show
void ReadLines(HANDLE pipe) {
    std::string pending;
    char buf[512];
    DWORD n;
    while (ReadFile(pipe, buf, sizeof buf, &n, nullptr) && n) {
        pending.append(buf, n);
        for (size_t nl; (nl = pending.find('\n')) != std::string::npos;) {
            std::string line = pending.substr(0, nl);
            pending.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            double f;
            int used = 0;
            if (std::sscanf(line.c_str(), "progress %lf %n", &f, &used) == 1 && used > 0) {
                g_progress = f;
                SetMessage(line.substr(size_t(used)));
            } else if (!line.empty()) {
                SetMessage(line);
            }
        }
    }
}

}  // namespace

void Start(Job job) {
    if (g_running) return;
    g_job = job;
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring cmd = L"\"" + std::wstring(self) + (job == Job::Textures ? L"\" --ai-textures" : L"\" --ai-movies") +
                       L" --no-launcher";
    if (!app::Get().game_path.empty()) cmd += L" \"--game=" + Wide(app::Get().game_path) + L"\"";
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE read = nullptr, write = nullptr;
    if (!CreatePipe(&read, &write, &sa, 0)) return;
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{sizeof si};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write;
    si.hStdError = write;  // the step sends its log elsewhere: anything here is a crash message worth showing
    PROCESS_INFORMATION pi{};
    bool ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                             app::ExeDir().wstring().c_str(), &si, &pi);
    CloseHandle(write);
    if (!ok) {
        CloseHandle(read);
        g_failed = true;
        SetMessage("Could not start the AI step.");
        return;
    }
    CloseHandle(pi.hThread);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_child = pi.hProcess;
    }
    g_progress = 0;
    g_failed = false;
    g_running = true;
    SetMessage("Starting");
    std::thread([read, proc = pi.hProcess] {
        ReadLines(read);
        CloseHandle(read);
        WaitForSingleObject(proc, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(proc, &code);
        {
            std::lock_guard<std::mutex> lock(g_mu);
            CloseHandle(proc);
            g_child = nullptr;
        }
        g_failed = code != 0;
        g_running = false;
    }).detach();
}

void Stop() {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_child) TerminateProcess(g_child, 1);  // its tools go with it (job object)
}

bool Running() { return g_running; }
Job Current() { return g_job; }
double Progress() { return g_progress; }
bool Failed() { return g_failed; }
bool TexturesReady() { return TexturesMade(); }

std::string Message() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_message;
}

}  // namespace ai_movies
