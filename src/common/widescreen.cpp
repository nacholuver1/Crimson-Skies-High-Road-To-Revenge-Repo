// True 16:9 and a field of view setting. On a widescreen output the runtime tells the game it has a widescreen TV and
// presents the frame 16:9, but the game still draws its 4:3 view, so the picture came out stretched. Before the camera
// builds its view (the call to 0x5DD10 at 0x2D6DA, which multiplies the view matrix's x, y and z outputs by its three
// arguments) it passes x and y scales of 1.0: 0.75 for x (4:3 over 16:9) widens the view instead.
//
// The fov setting divides both by the same factor, which widens (or narrows) the view without touching depth, so the
// draw distance and fog stay as they were.
//
// Found by crazyriddler's "Widescreen 16-9" patch for this game in xbased, which changes the x push; written here
// from what it changes, as a hook. The HUD and menus are drawn in 640x480 screen space and stay stretched.
#include "widescreen.h"

#include <cstring>

#include "app/app.h"
#include "cpu/x86_context.h"

namespace widescreen {
uint32_t g_fov = 100;
uint32_t Fov() { return g_fov < 80 ? 80 : g_fov > 130 ? 130 : g_fov; }
}  // namespace widescreen

// before "call 0x5DD10" at 0x2D6DA: [esp] = the x scale (the push at 0x2D6D3), [esp+4] = the y scale
void Widescreen_Scale(X86Context& ctx, uint8_t* base) {
    float zoom = 100.0f / float(widescreen::Fov());
    float x = zoom * (app::Widescreen() ? (4.0f / 3.0f) / (16.0f / 9.0f) : 1.0f), y = zoom;
    std::memcpy(base + ctx.esp, &x, 4);
    std::memcpy(base + ctx.esp + 4, &y, 4);
}
