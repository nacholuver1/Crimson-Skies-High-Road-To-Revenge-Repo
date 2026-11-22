#pragma once
#include <cstdint>

namespace input {
struct Pad;
}

namespace feedback {

extern bool g_triggers;  // the settings file's impulse_triggers
extern bool g_crash;     // the settings file's crash_rumble

void Pad(int port, input::Pad& pad);                     // GameInfo::pad: each polled pad
void Rumble(int port, uint16_t& left, uint16_t& right);  // GameInfo::rumble: each rumble the game sets

}  // namespace feedback
