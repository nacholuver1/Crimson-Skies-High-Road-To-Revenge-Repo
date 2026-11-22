// Controller feedback on top of the game's own rumble, which passes through as on the console.
//
// What the game sends (logged in play): the main guns pulse about 10 times a second at ~12,000 on both motors, each
// secondary shot is a ~0.5 s thump at ~23,000, the afterburner (Y) holds ~30,000, and a crash is a burst near 60,000
// that lasts only about 0.1 s, too short to feel.
//
// - Impulse triggers (impulse_triggers, on by default): the trigger motors of Xbox One / Series controllers follow the
//   game's rumble: the right one while RT (main guns) is held, the left one while LT (secondary) is held, both lightly
//   for the afterburner, and both for a crash.
// - Crash rumble (crash_rumble, on by default): a burst above 40,000 is held and faded over 0.35 s on the main motors.
#include "feedback.h"

#include <algorithm>
#include <chrono>

#include "core/input.h"

namespace feedback {

bool g_triggers = true;
bool g_crash = true;

namespace {

constexpr uint16_t kCrashLevel = 40000;  // a burst this strong is a collision
constexpr double kCrashSeconds = 0.35;

struct Port {
    bool lt = false, rt = false, y = false;  // held
    double crash_until = 0;
};
Port g_ports[4];

double Now() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return duration<double>(steady_clock::now() - t0).count();
}

uint16_t Clamp(double v) { return uint16_t(std::clamp(v, 0.0, 65535.0)); }

}  // namespace

void Pad(int port, input::Pad& p) {
    if (port < 0 || port > 3) return;
    Port& s = g_ports[port];
    s.lt = p.analog[6] > 0x40;
    s.rt = p.analog[7] > 0x40;
    s.y = p.analog[3] > 0x40;
}

void Rumble(int port, uint16_t& left, uint16_t& right) {
    if (port < 0 || port > 3) return;
    Port& s = g_ports[port];
    double t = Now();
    uint16_t level = std::max(left, right);
    if (level >= kCrashLevel) s.crash_until = t + kCrashSeconds;
    double crash = t < s.crash_until ? 65535.0 * (s.crash_until - t) / kCrashSeconds : 0.0;
    if (g_crash && crash > 0) {
        left = std::max(left, Clamp(crash));
        right = std::max(right, Clamp(crash));
    }
    if (!g_triggers) return;
    // the trigger motors are weaker than the main ones: the game's levels are doubled for them
    double lt = s.lt ? level * 2.0 : 0.0, rt = s.rt ? level * 2.0 : 0.0;
    if (s.y) lt = rt = std::max({lt, rt, level * 0.6});
    lt = std::max(lt, crash);
    rt = std::max(rt, crash);
    // the game sets its rumble every frame: each setting lasts a little longer than that and the next one replaces it
    input::RumbleTriggers(port, Clamp(lt), Clamp(rt), 100);
}

}  // namespace feedback
