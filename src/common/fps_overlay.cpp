// On-screen frame rate: F2 shows or hides it, show_fps = true in settings.toml starts with it on.
// fps is the renderer's count of presented frames over the last second; the worst frame is the longest gap between
// two frames in that second, which shows a hitch the average hides.
#include "fps_overlay.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <imgui.h>

namespace fps_overlay {

bool g_show = false;

namespace {
float g_worst_ms = 0, g_worst_shown = 0, g_window_s = 0;
}

bool Message(void*, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    if (msg == WM_KEYDOWN && wp == VK_F2 && !(lp & (1 << 30))) {  // first press, not the key's auto-repeat
        g_show = !g_show;
        *result = 0;
        return true;
    }
    return false;
}

void Draw(double fps) {
    float dt_ms = ImGui::GetIO().DeltaTime * 1000.0f;
    g_worst_ms = std::max(g_worst_ms, dt_ms);
    g_window_s += dt_ms / 1000.0f;
    if (g_window_s >= 1.0f) {
        g_worst_shown = g_worst_ms;
        g_worst_ms = 0;
        g_window_s = 0;
    }
    if (!g_show) return;
    ImGui::SetNextWindowPos(ImVec2(8, 8));
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##fps", nullptr, flags)) {
        ImGui::Text("%.0f fps  %.1f ms", fps, fps > 0 ? 1000.0 / fps : 0.0);
        ImGui::Text("worst frame %.1f ms", g_worst_shown);
    }
    ImGui::End();
}

}  // namespace fps_overlay
