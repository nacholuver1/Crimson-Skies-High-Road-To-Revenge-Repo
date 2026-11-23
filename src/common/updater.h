#pragma once
// Updates from the project's GitHub releases: the launcher asks for the latest release, and on the player's word
// downloads its win64 zip, checks its SHA-256, puts the new files over the old ones and starts the new version.
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

}  // namespace updater
