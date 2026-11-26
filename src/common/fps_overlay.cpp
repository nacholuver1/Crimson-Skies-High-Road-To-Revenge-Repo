// On-screen frame rate: F2 shows or hides it, show_fps = true in settings.toml starts with it on. Also the short
// notes the runtime posts (app::PostNote): a screenshot saved with F12, a restart instead of a demo program.
// fps is the renderer's count of presented frames over the last second; the worst frame is the longest gap between
// two frames in that second, which shows a hitch the average hides.
#include "fps_overlay.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <imgui.h>
#include <string>

#include "app/app.h"

namespace fps_overlay {

bool g_show = false;

namespace {
float g_worst_ms = 0, g_worst_shown = 0, g_window_s = 0;
std::string g_note;
float g_note_s = 0;  // seconds the runtime's note (app::PostNote) stays up
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
    // the runtime's notes: a screenshot saved (F12), a restart in place of a program the port doesn't include
    if (std::string note = app::TakeNote(); !note.empty()) {
        g_note = std::move(note);
        g_note_s = g_note.size() > 60 ? 6.0f : 2.0f;
    }
    if (g_note_s > 0) {
        g_note_s -= dt_ms / 1000.0f;
        ImVec2 size = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, size.y - 24), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGuiWindowFlags note = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##note", nullptr, note)) ImGui::TextUnformatted(g_note.c_str());
        ImGui::End();
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
