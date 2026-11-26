#pragma once
// Updates from the project's GitHub releases: the launcher asks for the latest release, and on the player's word
// downloads its win64 zip, checks its SHA-256, puts the new files over the old ones and starts the new version.
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace updater {

extern bool g_check;  // settings.toml check_updates: ask GitHub when the launcher opens

enum class State { Idle, Checking, UpToDate, Available, Downloading, Installing, Ready, Failed };

const char* Version();      // this build's version, e.g. "1.8.1"
void Cleanup();             // at startup: removes what the last update left (the old files, the download)
void StartCheck();          // ask for the latest release in the background (not with --no-update-check)
void StartInstall();        // download and install the available release in the background
State GetState();
std::string Latest();       // the newer release's version once Available
std::string Message();      // a failure's reason, or the release's name
int Progress();             // download percent while Downloading
bool Relaunch();            // once Ready: start the new version with the same command line; false when it failed

// for tools fetched on demand: an HTTPS download to a file, kept only when its SHA-256 (lower-case hex) matches;
// progress gets the bytes so far and the total (0 when unknown)
bool Download(const std::string& url, const std::filesystem::path& to, const std::string& sha256,
              const std::function<void(uint64_t, uint64_t)>& progress, std::string& err);
// a zip unpacked into dir with Windows' tar.exe; only: one member's path inside the zip, empty for all
bool Unpack(const std::filesystem::path& zip, const std::filesystem::path& dir, const std::wstring& only = {});

}  // namespace updater
