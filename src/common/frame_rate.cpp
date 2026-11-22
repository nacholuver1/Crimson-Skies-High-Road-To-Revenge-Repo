// Frame rate unlock: game_fps in settings.toml, 30 (the original) to 240; 60 is the default here. The renderer's
// fps_cap is raised to game_fps when it is lower (0, unlimited, and 1, the monitor's refresh, are left alone).
//
// The game paces itself: a gate (0x12FB70) lets a frame through once the performance-counter time since the last
// one reaches a minimum period, which the device setup (0x1308D9, 0x130B23) loads from the constant 1/30 at 0x2C8368
// into the gate object (+8). The constant itself is left alone, as other code may share it; the hooks change what
// those two loads put in xmm0.
//
// The game then advances by the frame's time in whole milliseconds: the gate truncates it with _ftol2 at 0x12FBCD.
// At 30 fps the dropped fraction is 0.33 of 33.3 ms; at 60 it would be 0.67 of 16.7 ms (a 4% slower game), at 144
// 0.94 of 6.94 ms (14% slower). Above 30 fps the hook before that call puts the fraction back into the gate's time
// base (+0x10, in counter ticks at the frequency in +8), so it counts in the next frame.
//
// That keeps the game's clock right, but each frame still moves the world by whole milliseconds (0x3CFC0 sets the
// frame time 0x3219BC = ms * 0.001): at 144 fps steps of 6 and 7 ms, at 240 of 4 and 5 (a 20% longer step every few
// frames), uneven motion at high frame rates. Above 30 fps the hook at 0x3D044 gives it the
// frame's exact time instead.
//
// One spinning part (0x16B17A) turns by its speed times the constant 1/30 each frame, so it spun faster with the
// frame rate: the constant is set to the frame period too.
//
// Found by crazyriddler's frame rate patches for this game in xbased (tools/mods/crimsonskies_*fps*); written
// here from their description, as hooks.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "app/app.h"
#include "cpu/x86_context.h"
#include "frame_rate.h"

namespace frame_rate {
uint32_t g_fps = 60;

uint32_t Fps() { return g_fps < 30 ? 30 : g_fps > 240 ? 240 : g_fps; }

void Init() {
    uint32_t& cap = app::Get().fps_cap;
    if (cap != 0 && cap != app::kFpsCapRefresh && cap < Fps()) cap = Fps();
}
}  // namespace frame_rate

// after "movss xmm0, [0x2C8368]" at 0x1308D9 and 0x130B23
void FrameRate_Period(X86Context& ctx, uint8_t* base) {
    float period = 1.0f / float(frame_rate::Fps());
    ctx.xmm[0].f[0] = period;
    std::memcpy(base + 0x2C8368, &period, 4);
}

namespace {
double g_carry = 0;      // the ms fraction the last frame moved into the next one
double g_exact_ms = 0;   // this frame's exact time
int32_t g_whole_ms = -1; // and what the gate truncates it to
}  // namespace

// before "call _ftol2" at 0x12FBCD: st0 = the frame's time in ms, esi = the gate's timer
void FrameRate_CarryMs(X86Context& ctx, uint8_t* base) {
    if (frame_rate::Fps() <= 30) return;
    double ms = ctx.st[ctx.top & 7];
    if (!(ms > 0) || ms > 1000) return;
    double fraction = ms - std::floor(ms);
    int64_t freq, time_base;
    std::memcpy(&freq, base + ctx.esi + 8, 8);
    std::memcpy(&time_base, base + ctx.esi + 0x10, 8);
    if (freq <= 0) return;
    g_exact_ms = ms - g_carry;  // the time base was moved back by the last fraction: take it out again
    g_whole_ms = int32_t(std::floor(ms));
    g_carry = fraction;
    time_base -= int64_t(std::llround(fraction * double(freq) / 1000.0));
    std::memcpy(base + ctx.esi + 0x10, &time_base, 8);
}

// at 0x3D044 in 0x3CFC0: xmm0 = min(ms * 0.001, 0.5), about to become the frame time (0x3219BC) and be added to the
// game time (0x3219C0); 0x3219AC = the whole ms the gate gave this frame (another value, such as the fixed 500 of a
// paused game, is left alone)
void FrameRate_Dt(X86Context& ctx, uint8_t* base) {
    if (frame_rate::Fps() <= 30 || g_whole_ms < 0) return;
    int32_t whole;
    std::memcpy(&whole, base + 0x3219AC, 4);
    if (whole != g_whole_ms || !(g_exact_ms > 0)) return;
    float dt = float(std::min(g_exact_ms * 0.001, 0.5));
    ctx.xmm[0].f[0] = dt;
    std::memcpy(base + 0x3219B4, &dt, 4);
}
