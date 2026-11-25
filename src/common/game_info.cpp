#include "app/app.h"
#include "cheats.h"
#include "feedback.h"
#include "fps_overlay.h"
#include "frame_rate.h"
#include "kbm.h"
#include "launcher.h"
#include "updater.h"
#include "widescreen.h"

#include <cstdlib>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

namespace {
// --game <path> or --game=<path>: the game for this run (a disc image, or the folder with default.xbe), for
// launchers and shortcuts. It takes game_path's place; without it nothing changes.
void GameArgument() {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;
    std::wstring path;
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if (a == L"--game" && i + 1 < argc) path = argv[++i];
        else if (a.rfind(L"--game=", 0) == 0) path = a.substr(7);
    }
    LocalFree(argv);
    if (path.empty()) return;
    int n = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), int(path.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), int(path.size()), utf8.data(), n, nullptr, nullptr);
    app::Get().game_path = utf8;
}

// the launcher first, which may change any setting; then what depends on the settings
void Startup() {
    GameArgument();
    updater::Cleanup();  // what an update left: the previous version's files
    if (launcher::Wanted() && !launcher::Run()) std::exit(0);
    frame_rate::Init();
}

// window messages: the FPS overlay's key first, then the mouse
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    return fps_overlay::Message(hwnd, msg, wp, lp, result) || kbm::Message(hwnd, msg, wp, lp, result);
}

// each polled pad: keyboard and mouse added to their player, then the trigger rumble reads what is held
void Pad(int port, input::Pad& pad) {
    kbm::Pad(port, pad);
    feedback::Pad(port, pad);
}

// each frame's ImGui: the cursor rule, then the FPS overlay
void Ui(double fps) {
    kbm::Ui();
    fps_overlay::Draw(fps);
}

// each rumble the game sets: none while the keyboard and mouse are in use, else the stronger crashes and triggers
void Rumble(int port, uint16_t& left, uint16_t& right) {
    if (kbm::MuteRumble(port)) {
        left = right = 0;
        return;
    }
    feedback::Rumble(port, left, right);
}
}  // namespace

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Crimson Skies: High Road to Revenge",  // window and splash
    .short_title = L"Crimson Skies",      // message boxes
    .name = L"CrimsonSkies",              // window classes, crash dumps, handover event
    .log = "crimsonskies",                // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "CrimsonSkies"}},  // each recompiled xbe and its exe, for title launches
    .options = {{"show_fps", &fps_overlay::g_show, nullptr},  // F2 toggles it in game
                {"game_fps", nullptr, &frame_rate::g_fps},    // 30 (original) to 240
                {"camera_sway", nullptr, &frame_rate::g_sway},  // turbulence sway of the camera, percent
                {"launcher", &launcher::g_show, nullptr},     // the pre-game launcher
                {"check_updates", &updater::g_check, nullptr},  // the launcher asks GitHub for a newer release
                {"impulse_triggers", &feedback::g_triggers, nullptr},  // trigger motors follow the game's rumble
                {"crash_rumble", &feedback::g_crash, nullptr},         // collisions held and faded
                {"fov", nullptr, &widescreen::g_fov},
                {"input_device", nullptr, &kbm::g_device},        // 0 auto, 1 controller, 2 keyboard and mouse
                {"mouse_mode", nullptr, &kbm::g_mouse_mode},      // 0 off, 1 left stick, 2 right stick
                {"mouse_sensitivity", nullptr, &kbm::g_mouse},    // percent
                {"mouse_pitch", &kbm::g_mouse_pitch, nullptr},    // the mouse pitches as well as turns
                {"keyboard_player", nullptr, &kbm::g_player},     // 1-4: the player the keyboard and mouse fly as, 2-4 for local multiplayer
                {"cheat_health", &cheats::g_health, nullptr},     // the game's god cheat at every spawn
                {"cheat_ammo", &cheats::g_ammo, nullptr},         // secondary weapons stay full
                {"cheat_boost", &cheats::g_boost, nullptr},       // boost never runs out
                {"cheat_planes", &cheats::g_planes, nullptr},     // every plane unlocked (not saved)
                {"cheat_money", &cheats::g_money, nullptr},       // 999,999 money, spending costs nothing (not saved)
                {"cheat_tokens", &cheats::g_tokens, nullptr}},    // 999 upgrade tokens, upgrades cost none (not saved)
    .load = kbm::Load,                      // reads more from the settings file's toml::table
    .save = kbm::Save,                      // writes more into it
    .init = Startup,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = Message,      // window messages first; true takes one
    .ui = Ui,              // ImGui drawing each frame
    .pad = Pad,                 // changes a polled pad
    .rumble = Rumble,           // changes a rumble before it is set
};
