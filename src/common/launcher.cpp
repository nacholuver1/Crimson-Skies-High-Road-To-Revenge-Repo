// Pre-game launcher: a window before the game boots, where the settings that only apply at startup are picked.
// Shown unless launcher = false in settings.toml or --no-launcher is on the command line, and never on the game's own
// relaunches (it restarts its executable to load a mission).
//
// Look and behaviour follow the Blitz recomps' launcher, itself adapted from darkuni's recomp menus
// (https://github.com/darkuni/outrun-arcade-recomp, MIT License, Copyright (c) 2026 darkuni): a centred panel of
// "Label  < value >" rows, a note line for the selected row, Up/Down to pick, Left/Right/Enter or a click to change,
// keyboard, mouse or controller. Drawn with Dear ImGui on Direct3D 11 in a window of its own, gone before the game's
// opens. The device and window setup follow Dear ImGui's example_win32_directx11 (MIT License).
#include "launcher.h"
#include "kbm.h"
#include "widescreen.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <commdlg.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <d3d11.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#include "app/app.h"
#include "feedback.h"
#include "fps_overlay.h"
#include "frame_rate.h"

namespace launcher {

bool g_show = true;  // settings.toml launcher

namespace {

// colours: Crimson Skies red on near black
const ImU32 kWhite = IM_COL32(255, 255, 255, 255);
const ImU32 kGrey = IM_COL32(185, 175, 170, 255);
const ImU32 kAccent = IM_COL32(222, 60, 40, 255);
const ImU32 kPanel = IM_COL32(18, 10, 9, 238);
const ImU32 kHighlight = IM_COL32(120, 28, 20, 235);
const ImU32 kBackdrop = IM_COL32(8, 5, 5, 255);

struct Choice {
    uint32_t value;
    const char* label;
};

const Choice kDisplay[] = {{uint32_t(app::DisplayMode::Borderless), "Borderless"},
                           {uint32_t(app::DisplayMode::Fullscreen), "Fullscreen"},
                           {uint32_t(app::DisplayMode::Windowed), "Windowed"}};
const uint32_t kResolutions[][2] = {{0, 0},       {1280, 720},  {1600, 900},  {1920, 1080}, {2560, 1080},
                                    {2560, 1440}, {3440, 1440}, {3840, 1600}, {3840, 2160}, {5120, 1440}};
const Choice kRender[] = {{50, "50%"},  {75, "75%"},   {100, "100% (native)"}, {150, "150% (1.5x supersampling)"},
                          {200, "200% (2x supersampling)"}, {300, "300% (3x supersampling)"}};
const Choice kAniso[] = {{0, "Game default"}, {2, "2x"}, {4, "4x"}, {8, "8x"}, {16, "16x"}};
const Choice kFps[] = {{30, "30 (original)"}, {60, "60"}, {120, "120"}, {144, "144"}, {165, "165"}, {240, "240"}};
const Choice kFovs[] = {{80, "80%"}, {90, "90%"}, {100, "100% (original)"}, {110, "110%"}, {120, "120%"}, {130, "130%"}};
const Choice kSways[] = {{0, "Off"}, {25, "25%"}, {50, "50%"}, {75, "75%"}, {100, "100% (original)"}};
const Choice kDevices[] = {{0, "Auto"}, {1, "Controller"}, {2, "Keyboard and mouse"}};
const Choice kMouseModes[] = {{0, "Off"}, {1, "Left stick (steer)"}, {2, "Right stick (roll, maneuvers)"}};
const Choice kMice[] = {{50, "50%"}, {75, "75%"}, {100, "100%"}, {150, "150%"}, {200, "200%"}, {300, "300%"}};
const Choice kLanguages[] = {{1, "English"}, {3, "German (text and voices)"}, {4, "French (text and voices)"},
                             {7, "Korean (text)"}, {8, "Chinese, traditional (text)"}};
const Choice kAudio[] = {{0, "Automatic"}, {1, "Stereo"}, {2, "5.1 surround"}, {3, "Mono"}};

enum Row { kStart, kGame, kDisplayMode, kResolution, kRenderRes, kAntiAlias, kFiltering, kVideoUpscale, kVsync, kFrameRate, kFov, kHud, kSway, kFpsCounter, kDevice, kBindings, kMouseMode, kMouse, kMousePitch, kTriggers, kCrash,
           kAudioOut, kLanguage, kLauncherAtStart, kQuit, kRows };

template <size_t N>
int IndexOf(const Choice (&c)[N], uint32_t v) {
    for (size_t i = 0; i < N; i++)
        if (c[i].value == v) return int(i);
    return 0;
}
template <size_t N>
uint32_t Step(const Choice (&c)[N], uint32_t v, int d) {
    return c[(IndexOf(c, v) + d + int(N)) % int(N)].value;
}

HWND g_hwnd = nullptr;
int g_input_cooldown = 0;  // frames that ignore input after the file dialog closes

// a path as UTF-8 in a std::string (C++20's u8string is std::u8string)
std::string Utf8(const std::filesystem::path& p) {
    std::u8string u = p.u8string();
    return std::string(reinterpret_cast<const char*>(u.data()), u.size());
}

// the game the launcher found: game_path, else the assets folder; probed once per change
struct GameState {
    std::filesystem::path path;
    app::GameProbe probe = app::GameProbe::Missing;
    bool probed = false;
} g_game;

void ProbeGame() {
    const app::Settings& s = app::Get();
    g_game.path = s.game_path.empty() ? app::DefaultGamePath() : std::filesystem::u8path(s.game_path);
    g_game.probe = g_game.path.empty() ? app::GameProbe::Missing : app::ProbeGame(g_game.path);
    g_game.probed = true;
}

bool GameReady() { return g_game.probe != app::GameProbe::Missing; }

// a file dialog for the disc image or the extracted game's default.xbe; true when a game was picked
bool BrowseGame() {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW of{sizeof(of)};
    of.hwndOwner = g_hwnd;
    of.lpstrFilter = L"Crimson Skies disc image or default.xbe\0*.iso;default.xbe\0Disc image (*.iso)\0*.iso\0All files\0*.*\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    of.lpstrTitle = L"Choose your Crimson Skies: High Road to Revenge disc image (.iso) or default.xbe";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    bool picked = GetOpenFileNameW(&of) != 0;
    // the key or click that closed the dialog (Enter, the double-click's second click) must not reach the launcher:
    // it would select the Game row again and reopen the dialog
    MSG m;
    while (PeekMessageW(&m, g_hwnd, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {}
    while (PeekMessageW(&m, g_hwnd, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) {}
    ImGuiIO& io = ImGui::GetIO();
    io.ClearEventsQueue();
    io.ClearInputKeys();
    io.ClearInputMouse();
    g_input_cooldown = 15;
    if (!picked) return false;
    std::filesystem::path chosen(file);
    if (chosen.filename() == L"default.xbe") chosen = chosen.parent_path();  // an extracted game: its folder
    app::Get().game_path = Utf8(chosen);
    app::Save();
    ProbeGame();
    return true;
}

std::string ShortPath(const std::filesystem::path& p, size_t max) {
    std::string s = Utf8(p);
    return s.size() <= max ? s : "..." + s.substr(s.size() - (max - 3));
}

int ResolutionIndex(const app::Settings& s) {
    for (size_t i = 0; i < std::size(kResolutions); i++)
        if (kResolutions[i][0] == s.width && kResolutions[i][1] == s.height) return int(i);
    return 0;
}

const char* Label(int row) {
    switch (row) {
        case kStart: return "START GAME";
        case kGame: return "Game";
        case kDisplayMode: return "Display mode";
        case kResolution: return "Resolution";
        case kRenderRes: return "Render resolution";
        case kAntiAlias: return "Anti-aliasing";
        case kFiltering: return "Texture filtering";
        case kVideoUpscale: return "Video upscaling";
        case kVsync: return "VSync";
        case kFrameRate: return "Frame rate";
        case kFov: return "Field of view";
        case kHud: return "HUD";
        case kSway: return "Camera sway";
        case kFpsCounter: return "FPS counter";
        case kDevice: return "Input device";
        case kBindings: return "Key bindings";
        case kMouseMode: return "Mouse moves";
        case kMouse: return "Mouse sensitivity";
        case kMousePitch: return "Mouse pitch";
        case kTriggers: return "Trigger rumble";
        case kCrash: return "Crash rumble";
        case kAudioOut: return "Audio output";
        case kLanguage: return "Language";
        case kLauncherAtStart: return "Launcher at startup";
        case kQuit: return "Quit";
    }
    return "";
}

const char* Note(int row) {
    switch (row) {
        case kStart:
            return GameReady() ? "Start Crimson Skies: High Road to Revenge with these settings."
                               : "Choose your game first: the Game row below.";
        case kGame:
            switch (g_game.probe) {
                case app::GameProbe::Match: return "Your disc image or game folder. Enter or a click picks another.";
                case app::GameProbe::OtherRelease: return "A different release than this build was made for: it may crash. Enter to pick another.";
                default: return "Enter or a click: pick your disc image (.iso) or the extracted game's default.xbe.";
            }
        case kDisplayMode: return "Borderless fullscreen, exclusive fullscreen or a window.";
        case kResolution: return "The window's size. Desktop follows your screen.";
        case kRenderRes: return "Of the output height. Above 100% renders bigger and scales down: smooth edges, more GPU.";
        case kAntiAlias: return "FXAA smooths jagged edges for almost no cost. Pairs with supersampling.";
        case kFiltering: return "Anisotropic filtering: sharper textures at an angle, like the ground below you.";
        case kVideoUpscale: return "Cutscenes and stills: bicubic upscaling and deinterlacing instead of a plain stretch.";
        case kVsync: return "Wait for the monitor's refresh. Off can tear.";
        case kFrameRate: return "The game was made for 30. Higher is smoother; set it to your monitor's refresh.";
        case kSway: return "The camera's turbulence sway. Lower it if the motion makes you feel sick.";
        case kFov: return "How much of the world you see. Wider can help with motion sickness.";
        case kHud: return "On wide screens: Fit keeps the HUD and menus in shape, centered. Stretch fills the width.";
        case kFpsCounter: return "Frame rate in the corner. F2 shows or hides it in game.";
        case kDevice: return "Auto: both work, and the controller only vibrates when you used it last.";
        case kBindings: return "Enter or a click: choose the keys and mouse buttons for every control.";
        case kMouseMode: return "Right stick: a quick mouse move down does the Immelmann. Ctrl (rebindable) holds it.";
        case kMouse: return "How far moving the mouse pushes the stick.";
        case kMousePitch: return "With the mouse on the left stick: moving it up and down pitches the nose too.";
        case kTriggers: return "Xbox One / Series controllers: the triggers vibrate with the guns, afterburner and crashes.";
        case kCrash: return "Makes crashes a strong jolt; the game's own crash rumble is very short.";
        case kAudioOut: return "Automatic follows your Windows speaker setup.";
        case kLanguage: return "The game's language. Korean and Chinese have English voices.";
        case kLauncherAtStart: return "Off starts the game directly. Back on: launcher = true in settings.toml.";
        case kQuit: return "Close without starting the game.";
    }
    return "";
}

std::string Value(int row) {
    const app::Settings& s = app::Get();
    char b[64];
    switch (row) {
        case kGame:
            if (g_game.probe == app::GameProbe::Missing) return "Not found - choose...";
            return ShortPath(g_game.path, 44) + (g_game.probe == app::GameProbe::Match ? "" : "  (other release)");
        case kDisplayMode: return kDisplay[IndexOf(kDisplay, uint32_t(s.display_mode))].label;
        case kResolution: {
            int i = ResolutionIndex(s);
            if (!s.width || !s.height) return "Desktop";
            if (i == 0) std::snprintf(b, sizeof b, "%ux%u", s.width, s.height);
            else std::snprintf(b, sizeof b, "%ux%u", kResolutions[i][0], kResolutions[i][1]);
            return b;
        }
        case kRenderRes: return kRender[IndexOf(kRender, s.render_percent)].label;
        case kAntiAlias: return s.fxaa ? "FXAA" : "Off";
        case kFiltering: return kAniso[IndexOf(kAniso, s.aniso)].label;
        case kVideoUpscale: return s.upscale_videos ? "On" : "Off";
        case kVsync: return s.vsync ? "On" : "Off";
        case kFrameRate: return kFps[IndexOf(kFps, frame_rate::Fps())].label;
        case kSway: return kSways[IndexOf(kSways, std::min(frame_rate::g_sway, 100u))].label;
        case kFov: return kFovs[IndexOf(kFovs, widescreen::Fov())].label;
        case kHud: return s.fit_hud ? "Fit" : "Stretch";
        case kFpsCounter: return fps_overlay::g_show ? "On" : "Off";
        case kDevice: return kDevices[IndexOf(kDevices, kbm::g_device)].label;
        case kMouseMode: return kMouseModes[IndexOf(kMouseModes, kbm::g_mouse_mode)].label;
        case kMouse: return kMice[IndexOf(kMice, kbm::g_mouse ? kbm::g_mouse : 100)].label;
        case kMousePitch: return kbm::g_mouse_pitch ? "On" : "Off";
        case kTriggers: return feedback::g_triggers ? "On" : "Off";
        case kCrash: return feedback::g_crash ? "Stronger" : "Original";
        case kAudioOut: return kAudio[IndexOf(kAudio, s.audio_output)].label;
        case kLanguage: return kLanguages[IndexOf(kLanguages, s.language)].label;
        case kLauncherAtStart: return g_show ? "On" : "Off";
    }
    return "";
}

void Change(int row, int d) {
    app::Settings& s = app::Get();
    switch (row) {
        case kDisplayMode: s.display_mode = app::DisplayMode(Step(kDisplay, uint32_t(s.display_mode), d)); break;
        case kResolution: {
            int n = int(std::size(kResolutions)), i = (ResolutionIndex(s) + d + n) % n;
            s.width = kResolutions[i][0];
            s.height = kResolutions[i][1];
            break;
        }
        case kRenderRes: s.render_percent = Step(kRender, s.render_percent, d); break;
        case kAntiAlias: s.fxaa = !s.fxaa; break;
        case kFiltering: s.aniso = Step(kAniso, s.aniso, d); break;
        case kVideoUpscale: s.upscale_videos = !s.upscale_videos; break;
        case kVsync: s.vsync = !s.vsync; break;
        case kFrameRate: frame_rate::g_fps = Step(kFps, frame_rate::Fps(), d); break;
        case kSway: frame_rate::g_sway = Step(kSways, std::min(frame_rate::g_sway, 100u), d); break;
        case kFov: widescreen::g_fov = Step(kFovs, widescreen::Fov(), d); break;
        case kHud: s.fit_hud = !s.fit_hud; break;
        case kFpsCounter: fps_overlay::g_show = !fps_overlay::g_show; break;
        case kDevice: kbm::g_device = Step(kDevices, kbm::g_device, d); break;
        case kMouseMode: kbm::g_mouse_mode = Step(kMouseModes, kbm::g_mouse_mode, d); break;
        case kMouse: kbm::g_mouse = Step(kMice, kbm::g_mouse ? kbm::g_mouse : 100, d); break;
        case kMousePitch: kbm::g_mouse_pitch = !kbm::g_mouse_pitch; break;
        case kTriggers: feedback::g_triggers = !feedback::g_triggers; break;
        case kCrash: feedback::g_crash = !feedback::g_crash; break;
        case kAudioOut: s.audio_output = Step(kAudio, s.audio_output, d); break;
        case kLanguage: s.language = Step(kLanguages, s.language, d); break;
        case kLauncherAtStart: g_show = !g_show; break;
        default: return;
    }
    app::Save();
}

bool HasOption(int row) { return row != kStart && row != kGame && row != kQuit && row != kBindings; }

enum class Result { Pending, Start, Quit };

// the key bindings page: every keyboard and mouse control, chosen by pressing the key
bool g_bindings = false;      // the page is open
int g_bsel = 0;               // its row: the actions, then Reset, then Back
int g_capture = -1;           // the action waiting for a key
bool g_wait_release = false;  // a press that just acted must be let go before the next one counts
bool g_prev_down[256] = {};

void OpenBindings() {
    g_bindings = true;
    g_bsel = 0;
    g_capture = -1;
    g_wait_release = true;
}

// a key or mouse button pressed since the last frame, or 0
int NewKey() {
    int found = 0;
    for (int vk = 1; vk < 255; vk++) {
        if (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_LMENU ||
            vk == VK_RMENU)
            continue;  // the generic Shift / Ctrl / Alt are bound instead
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (down && !g_prev_down[vk] && !found) found = vk;
        g_prev_down[vk] = down;
    }
    return found;
}

bool AnyKeyDown() {
    for (int vk = 1; vk < 255; vk++)
        if (GetAsyncKeyState(vk) & 0x8000) return true;
    return false;
}

void DrawBindings(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto& acts = kbm::Actions();
    const int n = int(acts.size()), rows = n + 2, kReset = n, kBack = n + 1;
    const int per_col = (n + 1) / 2;

    bool idle = false;
    if (g_wait_release) {
        if (!AnyKeyDown()) {
            g_wait_release = false;
            NewKey();  // take in the current state
        }
    } else if (g_capture >= 0) {
        if (int vk = NewKey()) {
            if (vk == VK_DELETE) acts[g_capture].keys.clear();
            else acts[g_capture].keys = {vk};
            g_capture = -1;
            g_wait_release = true;
            app::Save();
        }
    } else {
        idle = true;
        auto pressed = [](ImGuiKey a, ImGuiKey b) { return ImGui::IsKeyPressed(a, false) || ImGui::IsKeyPressed(b, false); };
        if (pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp)) g_bsel = (g_bsel + rows - 1) % rows;
        if (pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown)) g_bsel = (g_bsel + 1) % rows;
        if (pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight)) {
            if (g_bsel < per_col && g_bsel + per_col < n) g_bsel += per_col;
            else if (g_bsel == kReset) g_bsel = kBack;
        }
        if (pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft)) {
            if (g_bsel >= per_col && g_bsel < n) g_bsel -= per_col;
            else if (g_bsel == kBack) g_bsel = kReset;
        }
        if (pressed(ImGuiKey_Escape, ImGuiKey_GamepadFaceRight)) {
            g_bindings = false;
            g_wait_release = true;
            return;
        }
        if (pressed(ImGuiKey_Enter, ImGuiKey_GamepadFaceDown)) {
            g_wait_release = true;
            if (g_bsel < n) {
                g_capture = g_bsel;
            } else if (g_bsel == kReset) {
                kbm::ResetBindings();
                app::Save();
            } else {
                g_bindings = false;
                return;
            }
        }
    }

    float fs = ImGui::GetFontSize();
    float pad = fs * 1.2f, title_h = fs * 3.6f, note_h = fs * 3.0f;
    float row_h = std::min(fs * 1.7f, (io.DisplaySize.y - fs * 2 - title_h - note_h - pad) / (per_col + 1.5f));
    float col_w = std::min(ImGui::CalcTextSize("Special moves (right stick click)   V, Middle mouse").x + fs,
                           (io.DisplaySize.x - fs * 2 - pad * 3) / 2);
    float pw = col_w * 2 + pad * 3;
    float ph = title_h + row_h * (per_col + 1.5f) + note_h + pad;
    ImVec2 p0((io.DisplaySize.x - pw) / 2, (io.DisplaySize.y - ph) / 2), p1(p0.x + pw, p0.y + ph);
    dl->AddRectFilled(p0, p1, kPanel, fs * 0.4f);
    dl->AddRect(p0, p1, kAccent, fs * 0.4f, 0, std::max(1.0f, scale));
    dl->AddText(ImGui::GetFont(), fs * 1.4f, ImVec2(p0.x + pad, p0.y + pad * 0.6f), kAccent, "KEY BINDINGS");

    for (int i = 0; i < rows; i++) {
        int col = i < n ? i / per_col : (i == kReset ? 0 : 1);
        float r = i < n ? float(i % per_col) : per_col + 0.4f;
        float x = p0.x + pad + col * (col_w + pad);
        ImVec2 a(x - pad * 0.3f, p0.y + title_h + r * row_h), b(x + col_w, a.y + row_h);
        bool hover = idle && ImGui::IsMouseHoveringRect(a, b, false);  // not clipped to ImGui's default window
        if (hover && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) g_bsel = i;
        if (i == g_bsel) dl->AddRectFilled(a, b, kHighlight, fs * 0.25f);
        float ty = a.y + (row_h - fs) / 2, tx = a.x + pad * 0.3f;
        ImU32 col_c = i == g_bsel ? kWhite : kGrey;
        if (i < n) {
            dl->AddText(ImVec2(tx, ty), col_c, acts[i].label);
            std::string v = g_capture == i ? std::string("press a key...") : kbm::KeysText(acts[i].keys);
            float vw = ImGui::CalcTextSize(v.c_str()).x;
            dl->AddText(ImVec2(b.x - pad * 0.3f - vw, ty), g_capture == i ? kAccent : col_c, v.c_str());
        } else {
            dl->AddText(ImVec2(tx, ty), col_c, i == kReset ? "Reset to defaults" : "Back");
        }
        if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            g_bsel = i;
            g_wait_release = true;
            if (i < n) {
                g_capture = i;
            } else if (i == kReset) {
                kbm::ResetBindings();
                app::Save();
            } else {
                g_bindings = false;
                return;
            }
        }
    }
    const char* note = g_capture >= 0 ? "Press the key or mouse button for this control. Delete leaves it unbound."
                                      : "Enter or a click, then press the key  -  Esc/B back";
    dl->AddText(ImVec2(p0.x + pad, p1.y - note_h + fs * 0.6f), kGrey, note);
}

// one frame of the panel; returns the player's choice once made
Result Draw(int& sel, float scale) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, kBackdrop);
    if (g_bindings) {
        DrawBindings(scale);
        return Result::Pending;
    }
    if (g_wait_release) {  // back from the page: the key or click that closed it is let go first
        if (AnyKeyDown()) return Result::Pending;
        g_wait_release = false;
    }

    // input: keyboard and the controller's d-pad / sticks through ImGui's navigation keys; none for a few frames
    // after the file dialog
    bool input = g_input_cooldown == 0;
    if (g_input_cooldown > 0) g_input_cooldown--;
    auto pressed = [input](ImGuiKey a, ImGuiKey b, ImGuiKey c) {
        return input && (ImGui::IsKeyPressed(a, false) || ImGui::IsKeyPressed(b, false) || ImGui::IsKeyPressed(c, false));
    };
    if (pressed(ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp)) sel = (sel + kRows - 1) % kRows;
    if (pressed(ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadLStickDown)) sel = (sel + 1) % kRows;
    if (HasOption(sel) && pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft)) Change(sel, -1);
    if (HasOption(sel) && pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight)) Change(sel, 1);
    bool accept = pressed(ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_GamepadFaceDown) ||
                  (input && ImGui::IsKeyPressed(ImGuiKey_Space, false));
    if (input && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)))
        return Result::Quit;

    float fs = ImGui::GetFontSize();
    float pad = fs * 1.6f, title_h = fs * 4.2f, note_h = fs * 3.2f;
    // rows as tall as the window allows, up to 1.9 lines each
    float row_h = std::min(fs * 1.9f, (io.DisplaySize.y - fs * 2 - title_h - note_h - pad) / kRows);
    float label_w = ImGui::CalcTextSize("Launcher at startup").x + fs * 3;
    float value_w = std::max(ImGui::CalcTextSize("< 200% (2x supersampling) >").x,
                             ImGui::CalcTextSize("...ssssssssssssssssssssssssssssssssssssssssss  (other release)").x) + fs;
    const char* kHint = "Up/Down choose  -  Left/Right change  -  Enter/A start  -  Esc/B quit";
    float text_w = ImGui::CalcTextSize(kHint).x;
    for (int r = 0; r < kRows; r++) text_w = std::max(text_w, ImGui::CalcTextSize(Note(r)).x);
    float pw = std::max(label_w + value_w, text_w) + pad * 2;
    float ph = title_h + row_h * kRows + note_h + pad;
    ImVec2 p0((io.DisplaySize.x - pw) / 2, (io.DisplaySize.y - ph) / 2), p1(p0.x + pw, p0.y + ph);
    dl->AddRectFilled(p0, p1, kPanel, fs * 0.4f);
    dl->AddRect(p0, p1, kAccent, fs * 0.4f, 0, std::max(1.0f, scale));

    ImFont* font = ImGui::GetFont();
    dl->AddText(font, fs * 1.6f, ImVec2(p0.x + pad, p0.y + pad * 0.6f), kAccent, "CRIMSON SKIES");
    dl->AddText(ImVec2(p0.x + pad, p0.y + pad * 0.6f + fs * 1.8f), kGrey, "High Road to Revenge  -  native PC recompilation");

    Result result = Result::Pending;
    for (int r = 0; r < kRows; r++) {
        ImVec2 a(p0.x + pad * 0.5f, p0.y + title_h + r * row_h), b(p1.x - pad * 0.5f, a.y + row_h);
        bool hover = input && ImGui::IsMouseHoveringRect(a, b, false);  // not clipped to ImGui's default window
        if (hover && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) sel = r;
        if (r == sel) dl->AddRectFilled(a, b, kHighlight, fs * 0.25f);
        float ty = a.y + (row_h - fs) / 2;
        bool action = r == kStart || r == kGame || r == kQuit || r == kBindings;
        ImU32 col = r == kStart ? (GameReady() ? kAccent : kGrey) : r == sel ? kWhite : kGrey;
        dl->AddText(ImVec2(p0.x + pad, ty), r == kStart && r == sel ? kWhite : col, Label(r));
        if (HasOption(r) || r == kGame || r == kBindings) {
            std::string v = r == kBindings ? std::string(">") : r == sel && r != kGame ? "<  " + Value(r) + "  >" : Value(r);
            float vw = ImGui::CalcTextSize(v.c_str()).x;
            ImU32 vc = r == kGame && g_game.probe != app::GameProbe::Match ? kAccent : r == sel ? kWhite : kGrey;
            dl->AddText(ImVec2(p1.x - pad - vw, ty), vc, v.c_str());
        }
        if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            sel = r;
            if (action) accept = true;
            else Change(r, io.MousePos.x < (a.x + b.x) / 2 ? -1 : 1);
        }
        if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && HasOption(r)) Change(r, -1);
    }
    if (accept) {
        if (sel == kStart && !GameReady()) sel = kGame;
        else if (sel == kStart) result = Result::Start;
        else if (sel == kGame && BrowseGame() && GameReady()) sel = kStart;  // next Enter starts the game
        else if (sel == kQuit) result = Result::Quit;
        else if (sel == kBindings) OpenBindings();
        else Change(sel, 1);
    }
    dl->AddText(ImVec2(p0.x + pad, p1.y - note_h + fs * 0.6f), kGrey, Note(sel));
    dl->AddText(ImVec2(p0.x + pad, p1.y - fs * 1.6f), IM_COL32(120, 110, 105, 255), kHint);
    return result;
}

}  // namespace

bool Wanted() {
    const wchar_t* cmd = GetCommandLineW();
    if (std::wcsstr(cmd, L"--relaunched") || std::wcsstr(cmd, L"--no-launcher")) return false;
    if (std::wcsstr(cmd, L"--launcher")) return true;
    return g_show;
}

namespace {

bool g_closed = false;

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp)) return 1;
    if (msg == WM_CLOSE) {
        g_closed = true;
        return 0;
    }
    if (msg == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_KEYMENU) return 0;  // Alt alone does not open the window menu
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

bool Run() {
    ImGui_ImplWin32_EnableDpiAwareness();
    float scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));
    if (scale <= 0) scale = 1;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CrimsonSkiesLauncher";
    RegisterClassExW(&wc);
    int w = int(1240 * scale), h = int(820 * scale);
    RECT rc{0, 0, w, h};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rc, style, FALSE);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Crimson Skies: High Road to Revenge", style,
                                (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2, ww, wh,
                                nullptr, nullptr, inst, nullptr);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* dc = nullptr;
    IDXGISwapChain* sc = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    D3D_FEATURE_LEVEL fl;
    if (!hwnd || FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                                      D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &dc))) {
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, inst);
        return true;  // no launcher: start the game as before
    }
    ID3D11Texture2D* bb = nullptr;
    sc->GetBuffer(0, IID_PPV_ARGS(&bb));
    dev->CreateRenderTargetView(bb, nullptr, &rtv);
    bb->Release();
    g_hwnd = hwnd;
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    ImGuiContext* prev = ImGui::GetCurrentContext();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImFontConfig fc;
    fc.SizePixels = 19.0f * scale;
    io.Fonts->AddFontDefault(&fc);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(dev, dc);

    ProbeGame();
    int sel = GameReady() ? kStart : kGame;
    Result result = Result::Pending;
    g_closed = false;
    while (result == Result::Pending) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (g_closed) result = Result::Quit;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (result == Result::Pending) result = Draw(sel, scale);
        ImGui::Render();
        const float clear[4] = {8 / 255.0f, 5 / 255.0f, 5 / 255.0f, 1};
        dc->OMSetRenderTargets(1, &rtv, nullptr);
        dc->ClearRenderTargetView(rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        sc->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(ctx);
    ImGui::SetCurrentContext(prev);
    rtv->Release();
    sc->Release();
    dc->Release();
    dev->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, inst);
    app::Save();
    return result == Result::Start;
}

}  // namespace launcher
