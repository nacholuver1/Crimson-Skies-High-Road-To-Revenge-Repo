#pragma once
#include <cstdint>

namespace frame_rate {
extern uint32_t g_fps;  // the settings file's game_fps: 30 (original) to 240
uint32_t Fps();         // g_fps clamped to that range
void Init();            // GameInfo::init: raises the renderer's fps_cap to game_fps
}  // namespace frame_rate
