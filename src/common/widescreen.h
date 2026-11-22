#pragma once
#include <cstdint>

namespace widescreen {
extern uint32_t g_fov;  // the settings file's fov: the view's width and height in percent of the original's
uint32_t Fov();         // g_fov clamped to 80..130
}  // namespace widescreen
