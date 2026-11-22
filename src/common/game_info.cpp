#include "app/app.h"
#include "feedback.h"
#include "fps_overlay.h"
#include "frame_rate.h"
#include "kbm.h"
#include "launcher.h"
#include "widescreen.h"

#include <cstdlib>

namespace {
// the launcher first, which may change any setting; then what depends on the settings
void Startup() {
    if (launcher::Wanted() && !launcher::Run()) std::exit(0);
    frame_rate::Init();
}

// window messages: the FPS overlay's key first, then the mouse
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    return fps_overlay::Message(hwnd, msg, wp, lp, result) || kbm::Message(hwnd, msg, wp, lp, result);
}

// each polled pad: keyboard and mouse added to player 1, then the trigger rumble reads what is held
void Pad(int port, input::Pad& pad) {
    kbm::Pad(port, pad);
    feedback::Pad(port, pad);
}

// each rumble the game sets: none while the keyboard and mouse are in use, else the stronger crashes and triggers
void Rumble(int port, uint16_t& left, uint16_t& right) {
    if (kbm::MuteRumble()) {
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
                {"impulse_triggers", &feedback::g_triggers, nullptr},  // trigger motors follow the game's rumble
                {"crash_rumble", &feedback::g_crash, nullptr},         // collisions held and faded
                {"fov", nullptr, &widescreen::g_fov},
                {"input_device", nullptr, &kbm::g_device},        // 0 auto, 1 controller, 2 keyboard and mouse
                {"mouse_sensitivity", nullptr, &kbm::g_mouse},    // mouse steering, percent; 0 turns it off
                {"mouse_pitch", &kbm::g_mouse_pitch, nullptr}},   // the mouse pitches as well as turns                 // field of view, percent of the original
    .load = kbm::Load,                      // reads more from the settings file's toml::table
    .save = kbm::Save,                      // writes more into it
    .init = Startup,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = Message,      // window messages first; true takes one
    .ui = fps_overlay::Draw,              // ImGui drawing each frame
    .pad = Pad,                 // changes a polled pad
    .rumble = Rumble,           // changes a rumble before it is set
};
