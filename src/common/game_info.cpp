#include "app/app.h"
#include "feedback.h"
#include "fps_overlay.h"
#include "frame_rate.h"
#include "launcher.h"
#include "widescreen.h"

#include <cstdlib>

namespace {
// the launcher first, which may change any setting; then what depends on the settings
void Startup() {
    if (launcher::Wanted() && !launcher::Run()) std::exit(0);
    frame_rate::Init();
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
                {"impulse_triggers", &feedback::g_triggers, nullptr},  // trigger motors follow the game's rumble
                {"crash_rumble", &feedback::g_crash, nullptr},         // collisions held and faded
                {"fov", nullptr, &widescreen::g_fov}},                 // field of view, percent of the original
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = Startup,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = fps_overlay::Message,      // window messages first; true takes one
    .ui = fps_overlay::Draw,              // ImGui drawing each frame
    .pad = feedback::Pad,                 // changes a polled pad
    .rumble = feedback::Rumble,           // changes a rumble before it is set
};
