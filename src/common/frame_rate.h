#pragma once
#include <cstdint>

namespace frame_rate {
extern uint32_t g_fps;  // the settings file's game_fps: 30 (original) to 240
uint32_t Fps();         // g_fps clamped to that range
extern uint32_t g_sway; // the settings file's camera_sway: the camera's turbulence sway in percent, 0 to 100
void Init();            // GameInfo::init: raises the renderer's fps_cap to game_fps
}  // namespace frame_rate
