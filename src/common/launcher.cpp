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
#include "updater.h"
#include "widescreen.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <commdlg.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
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
#include "cheats.h"
#include "feedback.h"
#include "fps_overlay.h"
#include "frame_rate.h"

namespace launcher {

bool g_show = true;  // settings.toml launcher

namespace {

// colours: the game's 1930s pulp-aviation look, brass and cream on dark walnut under a crimson dusk
const ImU32 kWhite = IM_COL32(246, 234, 206, 255);   // cream: the selected row
const ImU32 kGrey = IM_COL32(184, 162, 128, 255);    // faded tan: the other rows
const ImU32 kAccent = IM_COL32(236, 84, 56, 255);    // crimson: warnings, key capture
const ImU32 kGold = IM_COL32(222, 178, 96, 255);     // brass: titles, borders
const ImU32 kGoldDim = IM_COL32(150, 112, 58, 255);
const ImU32 kHint = IM_COL32(132, 112, 88, 255);
const ImU32 kShadow = IM_COL32(0, 0, 0, 170);

ImFont* g_title_font = nullptr;  // Georgia Bold when Windows has it, else the default font

// the backdrop: a dusk sky fading from crimson to near black, with art-deco sunburst rays rising from below
void DrawBackdrop(ImDrawList* dl, ImVec2 size) {
    ImU32 top = IM_COL32(58, 16, 12, 255), mid = IM_COL32(28, 12, 10, 255), bottom = IM_COL32(10, 7, 6, 255);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(size.x, size.y * 0.55f), top, top, mid, mid);
    dl->AddRectFilledMultiColor(ImVec2(0, size.y * 0.55f), size, mid, mid, bottom, bottom);
    ImVec2 c(size.x * 0.5f, size.y * 1.15f);
    float r = std::max(size.x, size.y) * 1.6f;
    const int kRays = 36;
    for (int i = 0; i < kRays; i += 2) {
        float a0 = 3.14159265f * (1.0f + float(i) / kRays), a1 = 3.14159265f * (1.0f + float(i + 1) / kRays);
        dl->AddTriangleFilled(c, ImVec2(c.x + std::cos(a0) * r, c.y + std::sin(a0) * r),
                              ImVec2(c.x + std::cos(a1) * r, c.y + std::sin(a1) * r), IM_COL32(255, 170, 90, 9));
    }
    // a vignette darkens the edges
    float e = size.x * 0.18f;
    ImU32 dark = IM_COL32(0, 0, 0, 150), clear = IM_COL32(0, 0, 0, 0);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(e, size.y), dark, clear, clear, dark);
    dl->AddRectFilledMultiColor(ImVec2(size.x - e, 0), size, clear, dark, dark, clear);
}

// a panel: dark walnut, a double brass border with stepped art-deco corners
void DrawPanel(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float fs, float scale) {
    float t = std::max(1.0f, scale);
    dl->AddRectFilled(ImVec2(p0.x + fs * 0.4f, p0.y + fs * 0.5f), ImVec2(p1.x + fs * 0.4f, p1.y + fs * 0.5f),
                      IM_COL32(0, 0, 0, 110));  // drop shadow
    dl->AddRectFilledMultiColor(p0, p1, IM_COL32(40, 25, 18, 245), IM_COL32(40, 25, 18, 245),
                                IM_COL32(20, 12, 9, 245), IM_COL32(20, 12, 9, 245));
    dl->AddRect(p0, p1, kGold, 0, 0, t * 2);
    float in = fs * 0.35f;
    dl->AddRect(ImVec2(p0.x + in, p0.y + in), ImVec2(p1.x - in, p1.y - in), kGoldDim, 0, 0, t);
    // stepped corners: three brass steps at each corner
    float s = fs * 0.45f;
    for (int corner = 0; corner < 4; corner++) {
        float sx = corner & 1 ? -1.0f : 1.0f, sy = corner & 2 ? -1.0f : 1.0f;
        ImVec2 o(corner & 1 ? p1.x - in : p0.x + in, corner & 2 ? p1.y - in : p0.y + in);
        for (int k = 1; k <= 3; k++) {
            ImVec2 pts[3] = {ImVec2(o.x + sx * s * k, o.y), ImVec2(o.x + sx * s * k, o.y + sy * s * (4 - k)),
                             ImVec2(o.x, o.y + sy * s * (4 - k))};
            dl->AddPolyline(pts, 3, k == 3 ? kGold : kGoldDim, 0, t);
        }
    }
}

// a brass rule with a diamond in the middle
void DrawRule(ImDrawList* dl, float x0, float x1, float y, float fs, float scale) {
    float t = std::max(1.0f, scale), m = (x0 + x1) / 2, d = fs * 0.28f;
    dl->AddLine(ImVec2(x0, y), ImVec2(m - d * 2, y), kGoldDim, t);
    dl->AddLine(ImVec2(m + d * 2, y), ImVec2(x1, y), kGoldDim, t);
    dl->AddQuadFilled(ImVec2(m, y - d), ImVec2(m + d, y), ImVec2(m, y + d), ImVec2(m - d, y), kGold);
}

// the selected row: a crimson bar fading to the right, a brass edge on the left
void DrawHighlight(ImDrawList* dl, ImVec2 a, ImVec2 b, float fs, float scale) {
    ImU32 l = IM_COL32(150, 30, 22, 235), r = IM_COL32(70, 16, 12, 200);
    dl->AddRectFilledMultiColor(a, b, l, r, r, l);
    dl->AddRectFilled(a, ImVec2(a.x + std::max(2.0f, fs * 0.18f), b.y), kGold);
    dl->AddLine(ImVec2(a.x, b.y - 1), ImVec2(b.x, b.y - 1), IM_COL32(222, 178, 96, 90), std::max(1.0f, scale));
}

// letter-spaced capitals, centred on x
void DrawSpaced(ImDrawList* dl, ImFont* font, float size, float cx, float y, ImU32 col, const char* text, float gap) {
    float w = 0;
    for (const char* c = text; *c; c++) w += font->CalcTextSizeA(size, FLT_MAX, 0, c, c + 1).x + gap;
    float x = cx - (w - gap) / 2;
    for (const char* c = text; *c; c++) {
        dl->AddText(font, size, ImVec2(x, y), col, c, c + 1);
        x += font->CalcTextSizeA(size, FLT_MAX, 0, c, c + 1).x + gap;
    }
}

// the title block: CRIMSON SKIES in brass on a crimson ribbon with deco wings, the subtitle below
void DrawTitle(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float fs, float scale, const char* title, const char* sub) {
    ImFont* tf = g_title_font ? g_title_font : ImGui::GetFont();
    float ts = fs * 1.9f, cx = (p0.x + p1.x) / 2, y = p0.y + fs * 1.0f;
    float tw = 0;
    for (const char* c = title; *c; c++) tw += tf->CalcTextSizeA(ts, FLT_MAX, 0, c, c + 1).x + fs * 0.25f;
    float rh = ts * 1.2f, rw = tw / 2 + fs * 1.4f;
    ImVec2 r0(cx - rw, y - ts * 0.1f), r1(cx + rw, y - ts * 0.1f + rh);
    float notch = rh * 0.5f;
    // the ribbon's tails with a notch, then its band
    ImU32 deep = IM_COL32(96, 16, 12, 255), band = IM_COL32(150, 26, 20, 255);
    for (int side = -1; side <= 1; side += 2) {
        float x0 = side < 0 ? r0.x : r1.x, x1 = x0 + side * fs * 1.6f;
        float top = r0.y + rh * 0.2f, bot = r1.y + rh * 0.2f, mid = (top + bot) / 2, xn = x1 - side * notch * 0.7f;
        ImVec2 upper[4] = {ImVec2(x0, top), ImVec2(x1, top), ImVec2(xn, mid), ImVec2(x0, mid)};
        ImVec2 lower[4] = {ImVec2(x0, mid), ImVec2(xn, mid), ImVec2(x1, bot), ImVec2(x0, bot)};
        dl->AddConvexPolyFilled(upper, 4, deep);
        dl->AddConvexPolyFilled(lower, 4, deep);
        // the wings: three brass lines sweeping out from the ribbon
        for (int k = 0; k < 3; k++) {
            float wy = r0.y + rh * (0.35f + k * 0.15f), len = fs * (5.0f - k * 1.2f);
            dl->AddLine(ImVec2(x1 + side * fs * 0.5f, wy), ImVec2(x1 + side * (fs * 0.5f + len), wy), k == 0 ? kGold : kGoldDim,
                        std::max(1.0f, scale) * (k == 0 ? 2.0f : 1.0f));
        }
    }
    dl->AddRectFilledMultiColor(r0, r1, band, band, deep, deep);
    dl->AddRect(r0, r1, kGold, 0, 0, std::max(1.0f, scale));
    DrawSpaced(dl, tf, ts, cx + fs * 0.06f, y + fs * 0.08f, kShadow, title, fs * 0.25f);
    DrawSpaced(dl, tf, ts, cx, y, kGold, title, fs * 0.25f);
    DrawSpaced(dl, ImGui::GetFont(), fs * 0.85f, cx, r1.y + fs * 0.45f, kWhite, sub, fs * 0.22f);
}

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
const Choice kKeyPlayers[] = {{1, "Player 1"}, {2, "Player 2"}, {3, "Player 3"}, {4, "Player 4"}};
const Choice kMouseModes[] = {{0, "Off"}, {1, "Left stick (steer)"}, {2, "Right stick (roll, maneuvers)"}};
const Choice kMice[] = {{50, "50%"}, {75, "75%"}, {100, "100%"}, {150, "150%"}, {200, "200%"}, {300, "300%"}};
const Choice kLanguages[] = {{1, "English"}, {3, "German (text and voices)"}, {4, "French (text and voices)"},
                             {7, "Korean (text)"}, {8, "Chinese, traditional (text)"}};
const Choice kAudio[] = {{0, "Automatic"}, {1, "Stereo"}, {2, "5.1 surround"}, {3, "Mono"}};

enum Row { kStart, kGame, kUpdate, kDisplayMode, kResolution, kRenderRes, kAntiAlias, kFiltering, kTextures, kVideoUpscale, kVsync, kFrameRate, kFov, kHud, kSway, kFpsCounter, kDevice, kKeyPlayer, kBindings, kMouseMode, kMouse, kMousePitch, kTriggers, kCrash,
           kCheatHealth, kCheatAmmo, kCheatBoost, kCheatPlanes, kCheatMoney, kCheatTokens, kAudioOut, kLanguage, kLauncherAtStart, kQuit, kRows };

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
        case kUpdate: return "Updates";
        case kDisplayMode: return "Display mode";
        case kResolution: return "Resolution";
        case kRenderRes: return "Render resolution";
        case kAntiAlias: return "Anti-aliasing";
        case kFiltering: return "Texture filtering";
        case kTextures: return "Texture pack";
        case kVideoUpscale: return "Video upscaling";
        case kVsync: return "VSync";
        case kFrameRate: return "Frame rate";
        case kFov: return "Field of view";
        case kHud: return "HUD";
        case kSway: return "Camera sway";
        case kFpsCounter: return "FPS counter";
        case kDevice: return "Input device";
        case kKeyPlayer: return "Keyboard player";
        case kBindings: return "Key bindings";
        case kMouseMode: return "Mouse moves";
        case kMouse: return "Mouse sensitivity";
        case kMousePitch: return "Mouse pitch";
        case kTriggers: return "Trigger rumble";
        case kCrash: return "Crash rumble";
        case kCheatHealth: return "Cheat: unlimited health";
        case kCheatAmmo: return "Cheat: unlimited ammo";
        case kCheatBoost: return "Cheat: unlimited boost";
        case kCheatPlanes: return "Cheat: all planes";
        case kCheatMoney: return "Cheat: unlimited money";
        case kCheatTokens: return "Cheat: unlimited tokens";
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
        case kUpdate: {
            static std::string note;  // the returned pointer outlives this frame's call
            switch (updater::GetState()) {
                case updater::State::Available:
                    note = "Enter or a click: download " + updater::Latest() + " and restart. Settings and saves stay.";
                    break;
                case updater::State::Downloading:
                case updater::State::Installing: note = "Updating: the launcher restarts when it is done."; break;
                case updater::State::Failed: note = updater::Message() + "  Enter to try again."; break;
                default:
                    note = updater::g_check ? "Checks GitHub for a newer release when the launcher opens. Enter checks now."
                                            : "Off: no update checks. Enter checks once now.";
            }
            return note.c_str();
        }
        case kDisplayMode: return "Borderless fullscreen, exclusive fullscreen or a window.";
        case kResolution: return "The window's size. Desktop follows your screen.";
        case kRenderRes: return "Of the output height. Above 100% renders bigger and scales down: smooth edges, more GPU.";
        case kAntiAlias: return "FXAA smooths jagged edges for almost no cost. Pairs with supersampling.";
        case kFiltering: return "Anisotropic filtering: sharper textures at an angle, like the ground below you.";
        case kTextures: return "Images in textures\\replacement replace the game's. Save writes the game's to textures\\dumps.";
        case kVideoUpscale: return "Cutscenes and stills: bicubic upscaling and deinterlacing instead of a plain stretch.";
        case kVsync: return "Wait for the monitor's refresh. Off can tear.";
        case kFrameRate: return "The game was made for 30. Higher is smoother; set it to your monitor's refresh.";
        case kSway: return "The camera's turbulence sway. Lower it if the motion makes you feel sick.";
        case kFov: return "How much of the world you see. Wider can help with motion sickness.";
        case kHud: return "On wide screens: Fit keeps the HUD and menus in shape, centered. Stretch fills the width.";
        case kFpsCounter: return "Frame rate in the corner. F2 shows or hides it in game.";
        case kDevice: return "Auto: both work, and the controller only vibrates when you used it last.";
        case kKeyPlayer: return "Local multiplayer: Player 2 puts the keyboard and mouse beside controller 1 (player 1).";
        case kBindings: return "Enter or a click: choose the keys for every control, and one for each aerial maneuver.";
        case kMouseMode: return "Right stick: a quick mouse move down does the Immelmann. Ctrl (rebindable) holds it.";
        case kMouse: return "How far moving the mouse pushes the stick.";
        case kMousePitch: return "With the mouse on the left stick: moving it up and down pitches the nose too.";
        case kTriggers: return "Xbox One / Series controllers: the triggers vibrate with the guns, afterburner and crashes.";
        case kCrash: return "Makes crashes a strong jolt; the game's own crash rumble is very short.";
        case kCheatHealth: return "The game's own god cheat, on from the next time you spawn.";
        case kCheatAmmo: return "Your secondary weapon (rockets and the like) never runs out.";
        case kCheatBoost: return "Your afterburner never runs out.";
        case kCheatPlanes: return "Every plane unlocked and upgradable. Your save keeps its own unlocks, plus any upgrades you buy.";
        case kCheatMoney: return "999,999 money and buying costs nothing. Your save keeps its real money.";
        case kCheatTokens: return "999 upgrade tokens and upgrades cost none. Your save keeps its real tokens.";
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
        case kUpdate:
            switch (updater::GetState()) {
                case updater::State::Checking: return "Checking...";
                case updater::State::UpToDate:
                    return std::string(updater::g_check ? "Up to date (" : "Off (up to date, ") + updater::Version() + ")";
                case updater::State::Available: return "Version " + updater::Latest() + " available - install";
                case updater::State::Downloading:
                    std::snprintf(b, sizeof b, "Downloading... %d%%", updater::Progress());
                    return b;
                case updater::State::Installing: return "Installing...";
                case updater::State::Ready: return "Restarting...";
                case updater::State::Failed: return "Could not update - retry";
                default: return updater::g_check ? "Check at startup" : "Off";
            }
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
        case kTextures: return !s.custom_textures ? "Off" : s.dump_textures ? "On, save the game's textures" : "On";
        case kVideoUpscale: return s.upscale_videos ? "On" : "Off";
        case kVsync: return s.vsync ? "On" : "Off";
        case kFrameRate: return kFps[IndexOf(kFps, frame_rate::Fps())].label;
        case kSway: return kSways[IndexOf(kSways, std::min(frame_rate::g_sway, 100u))].label;
        case kFov: return kFovs[IndexOf(kFovs, widescreen::Fov())].label;
        case kHud: return s.fit_hud ? "Fit" : "Stretch";
        case kFpsCounter: return fps_overlay::g_show ? "On" : "Off";
        case kDevice: return kDevices[IndexOf(kDevices, kbm::g_device)].label;
        case kKeyPlayer: return kKeyPlayers[IndexOf(kKeyPlayers, kbm::g_player)].label;
        case kMouseMode: return kMouseModes[IndexOf(kMouseModes, kbm::g_mouse_mode)].label;
        case kMouse: return kMice[IndexOf(kMice, kbm::g_mouse ? kbm::g_mouse : 100)].label;
        case kMousePitch: return kbm::g_mouse_pitch ? "On" : "Off";
        case kTriggers: return feedback::g_triggers ? "On" : "Off";
        case kCrash: return feedback::g_crash ? "Stronger" : "Original";
        case kCheatHealth: return cheats::g_health ? "On" : "Off";
        case kCheatAmmo: return cheats::g_ammo ? "On" : "Off";
        case kCheatBoost: return cheats::g_boost ? "On" : "Off";
        case kCheatPlanes: return cheats::g_planes ? "On" : "Off";
        case kCheatMoney: return cheats::g_money ? "On" : "Off";
        case kCheatTokens: return cheats::g_tokens ? "On" : "Off";
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
        case kTextures: {  // Off, On, On + dump
            int i = !s.custom_textures ? 0 : s.dump_textures ? 2 : 1;
            i = (i + d + 3) % 3;
            s.custom_textures = i > 0;
            s.dump_textures = i == 2;
            break;
        }
        case kVideoUpscale: s.upscale_videos = !s.upscale_videos; break;
        case kVsync: s.vsync = !s.vsync; break;
        case kFrameRate: frame_rate::g_fps = Step(kFps, frame_rate::Fps(), d); break;
        case kSway: frame_rate::g_sway = Step(kSways, std::min(frame_rate::g_sway, 100u), d); break;
        case kFov: widescreen::g_fov = Step(kFovs, widescreen::Fov(), d); break;
        case kHud: s.fit_hud = !s.fit_hud; break;
        case kFpsCounter: fps_overlay::g_show = !fps_overlay::g_show; break;
        case kDevice: kbm::g_device = Step(kDevices, kbm::g_device, d); break;
        case kKeyPlayer: kbm::g_player = Step(kKeyPlayers, kbm::g_player, d); break;
        case kMouseMode: kbm::g_mouse_mode = Step(kMouseModes, kbm::g_mouse_mode, d); break;
        case kMouse: kbm::g_mouse = Step(kMice, kbm::g_mouse ? kbm::g_mouse : 100, d); break;
        case kMousePitch: kbm::g_mouse_pitch = !kbm::g_mouse_pitch; break;
        case kTriggers: feedback::g_triggers = !feedback::g_triggers; break;
        case kCrash: feedback::g_crash = !feedback::g_crash; break;
        case kCheatHealth: cheats::g_health = !cheats::g_health; break;
        case kCheatAmmo: cheats::g_ammo = !cheats::g_ammo; break;
        case kCheatBoost: cheats::g_boost = !cheats::g_boost; break;
        case kCheatPlanes: cheats::g_planes = !cheats::g_planes; break;
        case kCheatMoney: cheats::g_money = !cheats::g_money; break;
        case kCheatTokens: cheats::g_tokens = !cheats::g_tokens; break;
        case kAudioOut: s.audio_output = Step(kAudio, s.audio_output, d); break;
        case kLanguage: s.language = Step(kLanguages, s.language, d); break;
        case kLauncherAtStart: g_show = !g_show; break;
        case kUpdate:
            updater::g_check = !updater::g_check;
            if (updater::g_check && updater::GetState() == updater::State::Idle) updater::StartCheck();
            break;
        default: return;
    }
    app::Save();
}

bool HasOption(int row) { return row != kStart && row != kGame && row != kQuit && row != kBindings; }

enum class Result { Pending, Start, Quit };

bool g_restart = false;  // an update is in place: start the new version on the way out
bool g_offered = false;  // the popup was shown for a found release
bool g_popup = false;    // the update popup is open
int g_popup_sel = 0;     // its button: 0 Update now, 1 Later

bool Updating() {
    updater::State st = updater::GetState();
    return st == updater::State::Downloading || st == updater::State::Installing;
}

// Enter or a click on the Updates row: install what was found, else check now
void UpdateAction() {
    updater::State st = updater::GetState();
    if (st == updater::State::Available || (st == updater::State::Failed && !updater::Latest().empty())) updater::StartInstall();
    else if (st != updater::State::Checking && st != updater::State::Downloading && st != updater::State::Installing)
        updater::StartCheck();
}

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
    DrawPanel(dl, p0, p1, fs, scale);
    DrawSpaced(dl, g_title_font ? g_title_font : ImGui::GetFont(), fs * 1.4f, (p0.x + p1.x) / 2, p0.y + pad * 0.6f, kGold,
               "KEY BINDINGS", fs * 0.2f);
    DrawRule(dl, p0.x + pad, p1.x - pad, p0.y + title_h - fs * 0.6f, fs, scale);

    for (int i = 0; i < rows; i++) {
        int col = i < n ? i / per_col : (i == kReset ? 0 : 1);
        float r = i < n ? float(i % per_col) : per_col + 0.4f;
        float x = p0.x + pad + col * (col_w + pad);
        ImVec2 a(x - pad * 0.3f, p0.y + title_h + r * row_h), b(x + col_w, a.y + row_h);
        bool hover = idle && ImGui::IsMouseHoveringRect(a, b, false);  // not clipped to ImGui's default window
        if (hover && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) g_bsel = i;
        if (i == g_bsel) DrawHighlight(dl, a, b, fs, scale);
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
    DrawRule(dl, p0.x + pad, p1.x - pad, p1.y - note_h + fs * 0.1f, fs, scale);
    dl->AddText(ImVec2(p0.x + pad, p1.y - note_h + fs * 0.8f), kGrey, note);
}

// the popup over the launcher when a newer release is out: Update now or Later. True while it is open (it takes the
// input); the rows below stay drawn, dimmed.
void DrawUpdatePopup(float scale, bool input) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float fs = ImGui::GetFontSize(), pad = fs * 1.4f;
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 150));
    std::string head = "VERSION " + updater::Latest() + " IS OUT";
    // the release's name without its "v1.9.0: " prefix
    std::string what = updater::Message();
    if (size_t c = what.find(": "); c != std::string::npos && c < 12) what = what.substr(c + 2);
    std::string line1 = std::string("You have version ") + updater::Version() + (what.empty() ? "." : ".  New: " + what);
    const char* line2 = "Update now downloads it and restarts the launcher. Your settings, saves and game stay.";
    float pw = std::max(ImGui::CalcTextSize(line1.c_str()).x, ImGui::CalcTextSize(line2).x) + pad * 2;
    pw = std::max(pw, fs * 22);
    float ph = fs * 9.2f;
    ImVec2 p0((io.DisplaySize.x - pw) / 2, (io.DisplaySize.y - ph) / 2), p1(p0.x + pw, p0.y + ph);
    DrawPanel(dl, p0, p1, fs, scale);
    float cx = (p0.x + p1.x) / 2;
    DrawSpaced(dl, g_title_font ? g_title_font : ImGui::GetFont(), fs * 1.3f, cx, p0.y + fs * 0.9f, kGold, head.c_str(),
               fs * 0.15f);
    DrawRule(dl, p0.x + pad, p1.x - pad, p0.y + fs * 2.8f, fs, scale);
    dl->AddText(ImVec2(cx - ImGui::CalcTextSize(line1.c_str()).x / 2, p0.y + fs * 3.4f), kWhite, line1.c_str());
    dl->AddText(ImVec2(cx - ImGui::CalcTextSize(line2).x / 2, p0.y + fs * 4.7f), kGrey, line2);

    // input: Left/Right pick a button, Enter/A takes it, Esc/B is Later
    auto pressed = [input](ImGuiKey a, ImGuiKey b, ImGuiKey c) {
        return input && (ImGui::IsKeyPressed(a, false) || ImGui::IsKeyPressed(b, false) || ImGui::IsKeyPressed(c, false));
    };
    if (pressed(ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft)) g_popup_sel = 0;
    if (pressed(ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight)) g_popup_sel = 1;
    int choice = -1;
    if (pressed(ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_GamepadFaceDown) || (input && ImGui::IsKeyPressed(ImGuiKey_Space, false)))
        choice = g_popup_sel;
    if (pressed(ImGuiKey_Escape, ImGuiKey_GamepadFaceRight, ImGuiKey_Backspace)) choice = 1;

    const char* labels[2] = {"Update now", "Later"};
    float bw = fs * 8, bh = fs * 1.9f, gap = fs * 1.2f, by = p1.y - fs * 3.0f;
    for (int i = 0; i < 2; i++) {
        ImVec2 a(cx - bw - gap / 2 + i * (bw + gap), by), b(a.x + bw, by + bh);
        bool hover = input && ImGui::IsMouseHoveringRect(a, b, false);
        if (hover && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) g_popup_sel = i;
        if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) choice = i;
        if (i == g_popup_sel) DrawHighlight(dl, a, b, fs, scale);
        dl->AddRect(a, b, i == g_popup_sel ? kGold : kGoldDim, 0, 0, std::max(1.0f, scale));
        ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImVec2((a.x + b.x - ts.x) / 2, (a.y + b.y - ts.y) / 2), i == g_popup_sel ? kWhite : kGrey, labels[i]);
    }
    if (choice == 0) updater::StartInstall();
    if (choice >= 0) {
        g_popup = false;
        g_wait_release = true;  // the Enter or click that closed it does not reach the rows
    }
}

// one frame of the panel; returns the player's choice once made
Result Draw(int& sel, float scale) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    DrawBackdrop(dl, io.DisplaySize);
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
    bool popup_input = input;
    if (g_popup) input = false;  // the popup takes the keys, pad and mouse
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
    bool show_popup = g_popup;

    float fs = ImGui::GetFontSize();
    float pad = fs * 1.6f, title_h = fs * 5.4f, note_h = fs * 3.6f;
    // rows as tall as the window allows, up to 1.9 lines each
    float row_h = std::min(fs * 1.9f, (io.DisplaySize.y - fs * 2 - title_h - note_h - pad) / kRows);
    float label_w = ImGui::CalcTextSize("Launcher at startup").x + fs * 3;
    float value_w = std::max(ImGui::CalcTextSize("< 200% (2x supersampling) >").x,
                             ImGui::CalcTextSize("...ssssssssssssssssssssssssssssssssssssssssss  (other release)").x) + fs;
    const char* kHintText = "Up/Down choose  -  Left/Right change  -  Enter/A start  -  Esc/B quit";
    float text_w = ImGui::CalcTextSize(kHintText).x;
    for (int r = 0; r < kRows; r++) text_w = std::max(text_w, ImGui::CalcTextSize(Note(r)).x);
    float pw = std::max(label_w + value_w, text_w) + pad * 2;
    float ph = title_h + row_h * kRows + note_h + pad;
    ImVec2 p0((io.DisplaySize.x - pw) / 2, (io.DisplaySize.y - ph) / 2), p1(p0.x + pw, p0.y + ph);
    DrawPanel(dl, p0, p1, fs, scale);
    DrawTitle(dl, p0, p1, fs, scale, "CRIMSON SKIES", "HIGH ROAD TO REVENGE  -  PC RECOMPILATION");

    Result result = Result::Pending;
    for (int r = 0; r < kRows; r++) {
        ImVec2 a(p0.x + pad * 0.5f, p0.y + title_h + r * row_h), b(p1.x - pad * 0.5f, a.y + row_h);
        bool hover = input && ImGui::IsMouseHoveringRect(a, b, false);  // not clipped to ImGui's default window
        if (hover && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) sel = r;
        if (r == sel) DrawHighlight(dl, a, b, fs, scale);
        float ty = a.y + (row_h - fs) / 2;
        bool action = r == kStart || r == kGame || r == kQuit || r == kBindings || r == kUpdate;
        ImU32 col = r == kStart ? (GameReady() ? kGold : kGrey) : r == sel ? kWhite : kGrey;
        dl->AddText(ImVec2(p0.x + pad, ty), r == kStart && r == sel ? kWhite : col, Label(r));
        if (HasOption(r) || r == kGame || r == kBindings) {
            std::string v = r == kBindings ? std::string(">") : r == sel && r != kGame ? "<  " + Value(r) + "  >" : Value(r);
            float vw = ImGui::CalcTextSize(v.c_str()).x;
            ImU32 vc = r == kGame && g_game.probe != app::GameProbe::Match ? kAccent
                       : r == kUpdate && updater::GetState() == updater::State::Available ? kGold
                       : r == kUpdate && updater::GetState() == updater::State::Failed ? kAccent
                       : r == sel ? kWhite : kGrey;
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
        else if (sel == kStart && Updating()) sel = kUpdate;  // the update first: it may replace the compiler
        else if (sel == kStart) result = Result::Start;
        else if (sel == kGame && BrowseGame() && GameReady()) sel = kStart;  // next Enter starts the game
        else if (sel == kQuit) result = Result::Quit;
        else if (sel == kBindings) OpenBindings();
        else if (sel == kUpdate) UpdateAction();
        else Change(sel, 1);
    }
    // an update being written must finish first; once in place the launcher closes and the new version starts
    updater::State us = updater::GetState();
    if (us == updater::State::Installing && result != Result::Pending) result = Result::Pending;
    if (us == updater::State::Ready) {
        g_restart = true;
        result = Result::Quit;
    }
    // a newer release found: a popup offers it, once
    if (us == updater::State::Available && !g_offered) {
        g_offered = true;
        g_popup = true;
        g_popup_sel = 0;
        sel = kUpdate;
    }
    DrawRule(dl, p0.x + pad, p1.x - pad, p1.y - note_h + fs * 0.1f, fs, scale);
    dl->AddText(ImVec2(p0.x + pad, p1.y - note_h + fs * 0.8f), kGrey, Note(sel));
    dl->AddText(ImVec2(p0.x + pad, p1.y - fs * 1.6f), kHint, kHintText);
    std::string ver = std::string("v") + updater::Version();
    dl->AddText(ImVec2(p1.x - pad - ImGui::CalcTextSize(ver.c_str()).x, p1.y - fs * 1.6f), kHint, ver.c_str());
    if (show_popup) DrawUpdatePopup(scale, popup_input);
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
    int w = int(1240 * scale), h = int(880 * scale);
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
    // Georgia (on every Windows since XP) gives the game's 1930s serif look; the default font when it is missing
    wchar_t windir[MAX_PATH] = L"C:\\Windows";
    GetWindowsDirectoryW(windir, MAX_PATH);
    std::filesystem::path fonts = std::filesystem::path(windir) / "Fonts";
    auto load = [&](const char* name, float px) -> ImFont* {
        std::filesystem::path f = fonts / name;
        std::error_code ec;
        if (!std::filesystem::exists(f, ec)) return nullptr;
        return io.Fonts->AddFontFromFileTTF(Utf8(f).c_str(), px);
    };
    if (!load("georgia.ttf", 19.0f * scale)) io.Fonts->AddFontDefault(&fc);
    g_title_font = load("georgiab.ttf", 19.0f * scale * 2.0f);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(dev, dc);

    ProbeGame();
    if (updater::g_check) updater::StartCheck();
    int sel = GameReady() ? kStart : kGame;
    Result result = Result::Pending;
    g_closed = false;
    while (result == Result::Pending) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (g_closed && updater::GetState() != updater::State::Installing) result = Result::Quit;
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
    if (g_restart && !updater::Relaunch())
        MessageBoxW(nullptr, L"The update is installed. Start CrimsonSkies.exe again to play the new version.",
                    L"Crimson Skies", MB_OK | MB_ICONINFORMATION);
    return result == Result::Start;
}

}  // namespace launcher
