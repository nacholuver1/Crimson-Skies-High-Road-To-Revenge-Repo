// Keyboard and mouse as player 1's controller. The game only reads controllers, so the keys and the mouse are added to
// what port 1 reports (or make up a controller when none is plugged in); a real controller keeps working alongside.
//
// The game's flight actions (its InputMap): steer on the left stick, speed (up/down) and roll (left/right) on the right
// stick, primary fire RT, secondary LT, boost Y, brake B, the rest on A, X, black, white and the stick clicks.
// Every key is bound on the launcher's bindings page; the mouse turns (and, with mouse_pitch, pitches) through a
// virtual stick that it pushes and that recentres on its own.
//
// input_device: Auto lets both work and keeps a controller's rumble off while the keyboard and mouse were used last;
// Controller turns the keyboard and mouse off; Keyboard and mouse keeps the controller's rumble off.
#include "kbm.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <toml++/toml.hpp>

#include "app/app.h"
#include "core/input.h"

namespace kbm {

uint32_t g_device = kAuto;
uint32_t g_mouse = 0;  // off: the keys steer, the mouse buttons fire
bool g_mouse_pitch = false;

namespace {

enum : int {
    kNoseDown, kNoseUp, kTurnLeft, kTurnRight, kRollLeft, kRollRight, kSpeedUp, kSpeedDown, kPrimary, kSecondary,
    kBoost, kBrake, kSpecial, kButtonA, kButtonX, kBlack, kWhite, kLeftClick, kDpadUp, kDpadDown, kDpadLeft,
    kDpadRight, kStart, kBack, kAccept, kCancel, kActionCount
};

std::vector<Action> Defaults() {
    return {
        {"nose_down", "Nose down", {'W'}},
        {"nose_up", "Nose up", {'S'}},
        {"turn_left", "Turn left", {'A'}},
        {"turn_right", "Turn right", {'D'}},
        {"roll_left", "Roll left", {'Q'}},
        {"roll_right", "Roll right", {'E'}},
        {"speed_up", "Speed up", {'R'}},
        {"speed_down", "Slow down", {'F'}},
        {"primary", "Main guns (RT)", {VK_LBUTTON}},
        {"secondary", "Secondary weapon (LT)", {VK_RBUTTON}},
        {"boost", "Boost (Y)", {VK_SHIFT}},
        {"brake", "Brake (B)", {VK_SPACE}},
        {"special", "Special moves (right stick click)", {'V', VK_MBUTTON}},
        {"button_a", "A button", {'1'}},
        {"button_x", "X button", {'2'}},
        {"black", "Black button", {'Z'}},
        {"white", "White button", {'X'}},
        {"left_click", "Left stick click", {'C'}},
        {"dpad_up", "D-pad up", {VK_UP}},
        {"dpad_down", "D-pad down", {VK_DOWN}},
        {"dpad_left", "D-pad left", {VK_LEFT}},
        {"dpad_right", "D-pad right", {VK_RIGHT}},
        {"start", "START (pause)", {VK_ESCAPE}},
        {"back", "BACK", {VK_TAB}},
        {"accept", "Menu accept (A)", {VK_RETURN}},
        {"cancel", "Menu back (B)", {VK_BACK}},
    };
}

std::vector<Action> g_actions = Defaults();

std::atomic<int> g_dx{0}, g_dy{0};  // raw mouse motion since the last poll
std::atomic<int64_t> g_kbm_used{0}, g_pad_used{0};  // steady clock ms of the last use of each
bool g_registered = false;
bool g_clipped = false;
float g_sx = 0, g_sy = 0;  // the mouse's virtual stick, -1..1

constexpr float kCountsForFull = 400.0f;  // mouse counts that push the stick all the way at 100%
constexpr float kRecentre = 3.0f;         // per second: how fast the stick drifts back to the middle

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool On() { return g_device != kController; }

bool Front() {
    HWND w = static_cast<HWND>(app::Window());
    return w && GetForegroundWindow() == w;
}

bool Down(int a) {
    for (int vk : g_actions[a].keys)
        if (GetAsyncKeyState(vk) & 0x8000) return true;
    return false;
}

void Clip(HWND w, bool on) {
    if (on) {
        RECT r;
        GetClientRect(w, &r);
        POINT a{r.left, r.top}, b{r.right, r.bottom};
        ClientToScreen(w, &a);
        ClientToScreen(w, &b);
        RECT s{a.x, a.y, b.x, b.y};
        ClipCursor(&s);
    } else {
        ClipCursor(nullptr);
    }
    g_clipped = on;
}

int16_t Axis(float v) { return int16_t(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f)); }

// the larger deflection of the controller's and the keyboard's / mouse's
int16_t Merge(int16_t pad, int16_t ours) { return std::abs(int(ours)) > std::abs(int(pad)) ? ours : pad; }

bool PadActive(const input::Pad& p) {
    if (!p.connected) return false;
    constexpr int kDead = 8000;
    if (std::abs(int(p.lx)) > kDead || std::abs(int(p.ly)) > kDead || std::abs(int(p.rx)) > kDead ||
        std::abs(int(p.ry)) > kDead || p.buttons)
        return true;
    for (uint8_t a : p.analog)
        if (a > 0x40) return true;
    return false;
}

// names in the settings file and the launcher
struct Named {
    int vk;
    const char* name;
};
constexpr Named kNames[] = {
    {VK_LBUTTON, "Left mouse"}, {VK_RBUTTON, "Right mouse"}, {VK_MBUTTON, "Middle mouse"}, {VK_XBUTTON1, "Mouse 4"},
    {VK_XBUTTON2, "Mouse 5"},   {VK_SHIFT, "Shift"},         {VK_CONTROL, "Ctrl"},          {VK_MENU, "Alt"},
    {VK_SPACE, "Space"},        {VK_RETURN, "Enter"},        {VK_BACK, "Backspace"},        {VK_TAB, "Tab"},
    {VK_ESCAPE, "Esc"},         {VK_UP, "Up"},               {VK_DOWN, "Down"},             {VK_LEFT, "Left"},
    {VK_RIGHT, "Right"},        {VK_INSERT, "Insert"},       {VK_DELETE, "Delete"},         {VK_HOME, "Home"},
    {VK_END, "End"},            {VK_PRIOR, "Page Up"},       {VK_NEXT, "Page Down"},        {VK_CAPITAL, "Caps Lock"},
    {VK_OEM_1, ";"},            {VK_OEM_2, "/"},             {VK_OEM_3, "`"},               {VK_OEM_4, "["},
    {VK_OEM_5, "\\"},           {VK_OEM_6, "]"},             {VK_OEM_7, "'"},               {VK_OEM_COMMA, ","},
    {VK_OEM_PERIOD, "."},       {VK_OEM_MINUS, "-"},         {VK_OEM_PLUS, "="},
};

std::string KeyName(int vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, char(vk));
    if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return "Num " + std::to_string(vk - VK_NUMPAD0);
    for (const Named& n : kNames)
        if (n.vk == vk) return n.name;
    char b[16];
    std::snprintf(b, sizeof b, "Key %d", vk);
    return b;
}

int KeyFromName(const std::string& s) {
    for (int vk = 1; vk < 255; vk++)
        if (KeyName(vk) == s) return vk;
    if (s.rfind("Key ", 0) == 0) return std::atoi(s.c_str() + 4);
    return 0;
}

}  // namespace

std::vector<Action>& Actions() { return g_actions; }
void ResetBindings() { g_actions = Defaults(); }

std::string KeysText(const std::vector<int>& keys) {
    if (keys.empty()) return "-";
    std::string s;
    for (int vk : keys) s += (s.empty() ? "" : ", ") + KeyName(vk);
    return s;
}

void Load(const void* table) {
    const auto& t = *static_cast<const toml::table*>(table);
    const toml::table* c = t["controls"].as_table();
    if (!c) return;
    for (Action& a : g_actions) {
        const toml::array* arr = (*c)[a.key].as_array();
        if (!arr) continue;
        a.keys.clear();
        for (const auto& v : *arr)
            if (auto s = v.value<std::string>())
                if (int vk = KeyFromName(*s)) a.keys.push_back(vk);
    }
}

void Save(void* table) {
    auto& t = *static_cast<toml::table*>(table);
    toml::table c;
    for (const Action& a : g_actions) {
        toml::array arr;
        for (int vk : a.keys) arr.push_back(KeyName(vk));
        c.insert_or_assign(a.key, std::move(arr));
    }
    t.insert_or_assign("controls", std::move(c));
}

bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    HWND w = static_cast<HWND>(hwnd);
    if (!g_registered) {
        RAWINPUTDEVICE d{0x01, 0x02, 0, w};  // generic desktop, mouse
        g_registered = RegisterRawInputDevices(&d, 1, sizeof d) != FALSE;
    }
    switch (msg) {
        case WM_INPUT: {
            RAWINPUT ri;
            UINT size = sizeof ri;
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) !=
                    UINT(-1) &&
                ri.header.dwType == RIM_TYPEMOUSE && !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                g_dx += ri.data.mouse.lLastX;
                g_dy += ri.data.mouse.lLastY;
            }
            return false;  // DefWindowProc cleans up
        }
        case WM_SETCURSOR:
            if (On() && LOWORD(lp) == HTCLIENT && Front()) {
                SetCursor(nullptr);
                *result = TRUE;
                return true;
            }
            return false;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) Clip(w, false);
            return false;
        default:
            // keep the cursor in the window while steering, let it go otherwise
            if (On() && Front()) {
                if (!g_clipped) Clip(w, true);
            } else if (g_clipped) {
                Clip(w, false);
            }
            return false;
    }
}

void Pad(int port, input::Pad& p) {
    using namespace std::chrono;
    static auto last = steady_clock::now();
    if (port != 0) return;
    int64_t now_ms = NowMs();
    if (PadActive(p)) g_pad_used = now_ms;
    if (!On()) return;
    auto now = steady_clock::now();
    float dt = std::min(duration<float>(now - last).count(), 0.1f);
    last = now;
    int dx = g_dx.exchange(0), dy = g_dy.exchange(0);
    if (!Front()) {
        g_sx = g_sy = 0;
        return;
    }
    p.connected = true;
    bool used = dx != 0 || dy != 0;
    for (int a = 0; a < kActionCount && !used; a++) used = Down(a);
    if (used) g_kbm_used = now_ms;

    // left stick: the mouse's virtual stick (right turns right; up pushes the nose down, like a stick), then the keys
    if (g_mouse > 0) {
        float k = float(g_mouse) / 100.0f / kCountsForFull;
        g_sx = std::clamp(g_sx + dx * k, -1.0f, 1.0f);
        g_sy = g_mouse_pitch ? std::clamp(g_sy - dy * k, -1.0f, 1.0f) : 0.0f;
        float keep = std::exp(-kRecentre * dt);
        g_sx *= keep;
        g_sy *= keep;
    }
    float lx = g_sx + float(Down(kTurnRight)) - float(Down(kTurnLeft));
    float ly = g_sy + float(Down(kNoseDown)) - float(Down(kNoseUp));  // stick back pulls the nose up
    p.lx = Merge(p.lx, Axis(lx));
    p.ly = Merge(p.ly, Axis(ly));

    // right stick: speed up / down, roll left / right
    float rx = float(Down(kRollRight)) - float(Down(kRollLeft)), ry = float(Down(kSpeedUp)) - float(Down(kSpeedDown));
    p.rx = Merge(p.rx, Axis(rx));
    p.ry = Merge(p.ry, Axis(ry));

    // A B X Y black white LT RT
    auto press = [&](int i, bool on) {
        if (on) p.analog[i] = 0xFF;
    };
    press(0, Down(kButtonA) || Down(kAccept));
    press(1, Down(kBrake) || Down(kCancel));
    press(2, Down(kButtonX));
    press(3, Down(kBoost));
    press(4, Down(kBlack));
    press(5, Down(kWhite));
    press(6, Down(kSecondary));
    press(7, Down(kPrimary));

    // dpad up/down/left/right, start, back, left thumb, right thumb
    const int b[8] = {kDpadUp, kDpadDown, kDpadLeft, kDpadRight, kStart, kBack, kLeftClick, kSpecial};
    for (int i = 0; i < 8; i++)
        if (Down(b[i])) p.buttons |= uint16_t(1u << i);
}

bool MuteRumble() {
    if (g_device == kKeyboardMouse) return true;
    if (g_device == kController) return false;
    return g_kbm_used.load() > g_pad_used.load();
}

}  // namespace kbm
