#pragma once
#include <cstdint>

namespace fps_overlay {

extern bool g_show;  // the settings file's show_fps

// GameInfo::message: F2 toggles the overlay
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
// GameInfo::ui: draws it, inside the ImGui frame
void Draw(double fps);

}  // namespace fps_overlay
