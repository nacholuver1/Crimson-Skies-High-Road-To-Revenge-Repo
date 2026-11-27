// True 16:9 and a field of view setting. On a widescreen output the runtime tells the game it has a widescreen TV and
// presents the frame 16:9, but the game still draws its 4:3 view, so the picture came out stretched. Before the camera
// builds its view (the call to 0x5DD10 at 0x2D6DA, which multiplies the view matrix's x, y and z outputs by its three
// arguments) it passes x and y scales of 1.0: 0.75 for x (4:3 over 16:9) widens the view instead.
//
// The fov setting divides both by the same factor, which widens (or narrows) the view without touching depth, so the
// draw distance and fog stay as they were.
//
// Found by crazyriddler's "Widescreen 16-9" patch for this game in xbased, which changes the x push; written here
// from what it changes, as a hook. The HUD and menus are drawn in 640x480 screen space: the runtime's fit_hud
// narrows each of those draws back to its shape about the center.
#include "widescreen.h"

#include <cmath>
#include <cstring>

#include "app/app.h"
#include "cpu/x86_context.h"

namespace widescreen {
uint32_t g_fov = 100;
uint32_t Fov() { return g_fov < 80 ? 80 : g_fov > 130 ? 130 : g_fov; }
}  // namespace widescreen

// HUD markers (issue #10): the brackets on planes, the objective markers, the lead indicator and the like are 2D
// draws placed where the camera's world-to-screen (0x2B660, thiscall: out float x,y pairs, in xyz points, count; ret
// 0x0C) puts their target. With the widened view that is right for the stretched frame, but fit_hud then narrows
// every 2D draw toward the center (x' = c + (x - c) * k), which pulled the markers off their targets. For the HUD's
// callers only, the hook spreads x out by 1/k so the narrowing lands them back on target, keeping their shape. Not
// for the plane's boost lens (0xCFF73: a lens centre in the frame, drawn unnarrowed) or the other callers.
// Coordinates are relative to the viewport ([camera+0xE3C]+0x34: left, top, right, bottom; split-screen halves);
// -1, -1 means behind the camera and stays. A target in the wide screen's side bands now projects outside the
// 4:3 frame's 0..w, so a caller that tests on-screen-ness may treat it as off screen (to check in game).
static bool HudCaller(uint32_t ret) {
    switch (ret) {
        case 0x121189:  // the unit indicators (enemy / friend brackets): 0x121120
        case 0x121490:  // 0x1213B0, same class
        case 0x1215BD:  // 0x121540, same class
        case 0x115E6F:  // the HUD's activity / offscreen arrows (0x115E30)
        case 0x116BCB:  // ScenePromptUI::Render2D (0x1169D0)
        case 0xD013E:   // the plane's HUD element at +0xAB8 (0xD00E0)
        case 0xD3656:   // the plane's HUD marker sprite at +0xAEC (0xD3350)
            return true;
    }
    return false;
}

// The unit brackets never check for the behind-the-camera result: they round -1, -1 and draw the bracket at the
// viewport's top-left corner (issue #12). A unit counts as visible by its bounding sphere, so a wingman alongside and
// a little behind (its centre behind the camera plane, its sphere still in the wide view) left a bracket stuck there.
// For those callers a point behind the camera is moved far off screen, so the bracket is drawn outside the frame.
static bool BracketCaller(uint32_t ret) { return ret == 0x121189 || ret == 0x121490 || ret == 0x1215BD; }

// at "ret 0x0C" (0x2B7A9) of 0x2B660: ecx = the camera, [esp] = the caller, [esp+4] = out, [esp+0xC] = count
void Widescreen_HudProject(X86Context& ctx, uint8_t* base) {
    uint32_t ret, out, count, vp;
    std::memcpy(&ret, base + ctx.esp, 4);
    if (!HudCaller(ret)) return;
    std::memcpy(&out, base + ctx.esp + 4, 4);
    std::memcpy(&count, base + ctx.esp + 12, 4);
    if (!out || count > 64) return;
    if (BracketCaller(ret)) {
        for (uint32_t i = 0; i < count; i++) {
            float x, y;
            std::memcpy(&x, base + out + i * 8, 4);
            std::memcpy(&y, base + out + i * 8 + 4, 4);
            if (x == -1.0f && y == -1.0f) {
                const float far_off = -100000.0f;
                std::memcpy(base + out + i * 8, &far_off, 4);
                std::memcpy(base + out + i * 8 + 4, &far_off, 4);
            }
        }
    }
    if (!app::Get().fit_hud) return;
    float k = (4.0f / 3.0f) / app::PresentAspect();
    if (!(k > 0.1f && k < 0.999f)) return;  // PresentAspect is 4:3 to 32:9
    std::memcpy(&vp, base + ctx.ecx + 0xE3C, 4);
    if (!out || !vp || count > 64) return;
    int32_t left;
    std::memcpy(&left, base + vp + 0x34, 4);
    // the frame's center: HudFit narrows about the middle of the 4:3 frame the 2D is drawn on, whose width the UI
    // code reads at [0x3219CC]+0x19178 (640)
    uint32_t screen;
    std::memcpy(&screen, base + 0x3219CC, 4);
    int32_t w = 640;
    if (screen) std::memcpy(&w, base + screen + 0x19178, 4);
    if (w <= 0 || w > 8192) w = 640;
    const float c = float(w) / 2;
    for (uint32_t i = 0; i < count; i++) {
        float x, y;
        std::memcpy(&x, base + out + i * 8, 4);
        std::memcpy(&y, base + out + i * 8 + 4, 4);
        if ((x == -1.0f && y == -1.0f) || !std::isfinite(x)) continue;  // 0x2B660 writes exactly -1, -1 when behind
        float abs_x = float(left) + x;
        x = c + (abs_x - c) / k - float(left);
        std::memcpy(base + out + i * 8, &x, 4);
    }
}

// before "call 0x5DD10" at 0x2D6DA: [esp] = the x scale (the push at 0x2D6D3), [esp+4] = the y scale
void Widescreen_Scale(X86Context& ctx, uint8_t* base) {
    float zoom = 100.0f / float(widescreen::Fov());
    float x = zoom * (4.0f / 3.0f) / app::PresentAspect(), y = zoom;  // 0.75 at 16:9, 0.57 at 21:9
    std::memcpy(base + ctx.esp, &x, 4);
    std::memcpy(base + ctx.esp + 4, &y, 4);
}
