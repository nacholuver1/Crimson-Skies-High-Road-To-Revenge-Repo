#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace input {
struct Pad;
}

namespace kbm {

enum Device : uint32_t { kAuto = 0, kController = 1, kKeyboardMouse = 2 };
extern uint32_t g_device;   // the settings file's input_device
extern uint32_t g_mouse;    // mouse_sensitivity, percent
enum MouseMode : uint32_t { kMouseOff = 0, kMouseLeft = 1, kMouseRight = 2 };
extern uint32_t g_mouse_mode;  // mouse_mode: what moving the mouse does
extern bool g_mouse_pitch;  // mouse_pitch: the mouse also pitches (up/down), not only turns

// the bindable actions, in the order the launcher lists them
struct Action {
    const char* key;        // in the settings file's [controls]
    const char* label;      // in the launcher
    std::vector<int> keys;  // Windows virtual keys (mouse buttons included); any of them
};
std::vector<Action>& Actions();
void ResetBindings();
std::string KeysText(const std::vector<int>& keys);

// GameInfo::load / save: the [controls] table the launcher's bindings page writes
void Load(const void* table);
void Save(void* table);
// GameInfo::message: raw mouse motion, the hidden cursor kept in the window
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
// GameInfo::pad: player 1's controller with the keyboard and mouse added
void Pad(int port, input::Pad& pad);
// GameInfo::ui, inside the ImGui frame: stops ImGui from bringing the hidden cursor back
void Ui();
// true while a controller's rumble should stay off: keyboard and mouse chosen, or last used in Auto
bool MuteRumble();

}  // namespace kbm
