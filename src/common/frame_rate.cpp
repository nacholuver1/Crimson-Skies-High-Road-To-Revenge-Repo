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
// Found by crazyriddler's frame rate patches for this game in xbased (tools/mods/crimsonskies_*fps*); written
// here from their description, as hooks.
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
void FrameRate_Period(X86Context& ctx, uint8_t*) {
    ctx.xmm[0].f[0] = 1.0f / float(frame_rate::Fps());
}

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
    time_base -= int64_t(std::llround(fraction * double(freq) / 1000.0));
    std::memcpy(base + ctx.esi + 0x10, &time_base, 8);
}
