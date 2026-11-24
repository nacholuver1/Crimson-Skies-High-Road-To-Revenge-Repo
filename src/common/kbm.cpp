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
#include "imgui.h"

namespace kbm {

uint32_t g_device = kAuto;
uint32_t g_mouse = 100;
uint32_t g_mouse_mode = kMouseOff;  // off: the keys steer, the mouse buttons fire
bool g_mouse_pitch = false;

namespace {

enum : int {
    kNoseDown, kNoseUp, kTurnLeft, kTurnRight, kRollLeft, kRollRight, kSpeedUp, kSpeedDown, kPrimary, kSecondary,
    kBoost, kBrake, kSpecial, kButtonA, kButtonX, kBlack, kWhite, kLeftClick, kDpadUp, kDpadDown, kDpadLeft,
    kDpadRight, kStart, kBack, kAccept, kCancel, kMouseRightHold, kImmelmann, kSplitS, kSnapTurn, kPowerClimb,
    kBackStallLoop, kYoYo, kSnapRollLeft, kSnapRollRight, kDoubleSnapLeft, kDoubleSnapRight, kBarrelCw, kBarrelCcw,
    kDoubleBarrelCw, kDoubleBarrelCcw, kActionCount
};

// The maneuver keys play what the game reads as that maneuver: a click of the right stick (R3, the game only takes
// maneuvers in a short window after it) and the stick moves (left stick, right stick; up is +1, down -1). A step with
// hold set lasts as long as the key is held, at least ms; the others last exactly ms, so a "push" key held down
// doesn't turn into the "hold" maneuver.
struct Step {
    int ms;
    bool hold;
    float lx, ly, rx, ry;
};
struct Maneuver {
    int action;
    Step steps[4];
    int count;
};
constexpr int kPush = 200, kHold = 700;
const Maneuver kManeuvers[] = {
    {kSplitS, {{kPush, false, 0, 1, 0, 1}}, 1},
    {kSnapTurn, {{kPush, false, 0, 1, 0, -1}}, 1},
    {kPowerClimb, {{kPush, false, 0, -1, 0, 1}}, 1},
    // hold the left stick down, push the right stick up, then push the left stick down again
    {kBackStallLoop, {{kPush, false, 0, -1, 0, 1}, {500, true, 0, -1, 0, 0}, {150, false, 0, 0, 0, 0},
                      {kPush, false, 0, -1, 0, 0}}, 4},
    // the left stick down, then up as the nose rises, with the right stick up
    {kYoYo, {{kPush, false, 0, -1, 0, 1}, {250, false, 0, 1, 0, 1}}, 2},
    {kSnapRollLeft, {{kPush, false, -1, 0, -1, 0}}, 1},
    {kSnapRollRight, {{kPush, false, 1, 0, 1, 0}}, 1},
    {kDoubleSnapLeft, {{kHold, true, -1, 0, -1, 0}}, 1},
    {kDoubleSnapRight, {{kHold, true, 1, 0, 1, 0}}, 1},
    {kBarrelCw, {{kPush, false, -1, 0, 1, 0}}, 1},
    {kBarrelCcw, {{kPush, false, 1, 0, -1, 0}}, 1},
    {kDoubleBarrelCw, {{kHold, true, -1, 0, 1, 0}}, 1},
    {kDoubleBarrelCcw, {{kHold, true, 1, 0, -1, 0}}, 1},
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
        {"special", "R3 (right stick click)", {'V', VK_MBUTTON}},
        {"button_a", "A button", {'1'}},
        {"button_x", "X button", {'2'}},
        {"black", "Black button", {'Z'}},
        {"white", "White button", {'X'}},
        {"left_click", "L3 (left stick click)", {'C'}},
        {"dpad_up", "D-pad up", {VK_UP}},
        {"dpad_down", "D-pad down", {VK_DOWN}},
        {"dpad_left", "D-pad left", {VK_LEFT}},
        {"dpad_right", "D-pad right", {VK_RIGHT}},
        {"start", "START (pause)", {VK_ESCAPE}},
        {"back", "BACK", {VK_TAB}},
        {"accept", "Menu accept (A)", {VK_RETURN}},
        {"cancel", "Menu back (B)", {VK_BACK}},
        {"mouse_right_stick", "Mouse moves right stick (hold)", {VK_CONTROL}},
        {"immelmann", "Immelmann (sticks back + R3)", {VK_XBUTTON1}},
        {"split_s", "Split S", {'3'}},
        {"snap_turn", "Snap Turn", {'4'}},
        {"power_climb", "Power Climb", {'5'}},
        {"back_stall_loop", "Back Stall Loop", {'6'}},
        {"yo_yo", "High-Speed Yo-Yo", {'7'}},
        {"snap_roll_left", "Snap Roll Left", {'T'}},
        {"snap_roll_right", "Snap Roll Right", {'Y'}},
        {"double_snap_left", "Double Snap Left", {'G'}},
        {"double_snap_right", "Double Snap Right", {'H'}},
        {"barrel_cw", "Barrel Roll (clockwise)", {'U'}},
        {"barrel_ccw", "Barrel Roll (counter-cw)", {'I'}},
        {"double_barrel_cw", "Double Barrel (clockwise)", {'J'}},
        {"double_barrel_ccw", "Double Barrel (counter-cw)", {'K'}},
    };
}

std::vector<Action> g_actions = Defaults();

std::atomic<int> g_dx{0}, g_dy{0};  // raw mouse motion since the last poll
std::atomic<int64_t> g_kbm_used{0}, g_pad_used{0};  // steady clock ms of the last use of each
bool g_registered = false;
bool g_clipped = false;
bool g_hidden = false;            // the window thread's cursor is hidden (ShowCursor)
std::atomic<bool> g_hide{false};  // keyboard and mouse in use with the game in front
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
            if (LOWORD(wp) == WA_INACTIVE) {
                Clip(w, false);
                if (g_hidden) {
                    ShowCursor(TRUE);
                    g_hidden = false;
                }
                g_hide = false;
            }
            return false;
        default: {
            // keyboard and mouse in use with the game in front: the cursor hidden and kept in the window
            bool hide = On() && Front();
            g_hide = hide;
            if (hide && !g_clipped) Clip(w, true);
            if (!hide && g_clipped) Clip(w, false);
            if (hide != g_hidden) {
                ShowCursor(hide ? FALSE : TRUE);
                g_hidden = hide;
            }
            return false;
        }
    }
}

// the maneuver being played, if any
static const Maneuver* playing = nullptr;
static int64_t step_since = 0, started = 0;
static int step = 0;
static bool was_down[kActionCount] = {};

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
        playing = nullptr;  // a maneuver doesn't carry on after the window comes back
        return;
    }
    p.connected = true;
    bool used = dx != 0 || dy != 0;
    for (int a = 0; a < kActionCount && !used; a++) used = Down(a);
    if (used) g_kbm_used = now_ms;

    // the mouse's virtual stick: moving the mouse pushes it (up is stick up), it recentres on its own. It is the left
    // stick (steering: right turns right, up pushes the nose down) or, chosen in mouse_mode or while the hold key is
    // down, the right stick (roll, speed and the maneuvers: mouse down flicks it down for an Immelmann)
    bool right = g_mouse_mode == kMouseRight || Down(kMouseRightHold);
    bool moves = right || g_mouse_mode == kMouseLeft;
    if (moves) {
        float k = float(g_mouse ? g_mouse : 100) / 100.0f / kCountsForFull;  // 0: an older settings file's "off"
        g_sx = std::clamp(g_sx + dx * k, -1.0f, 1.0f);
        g_sy = right || g_mouse_pitch ? std::clamp(g_sy - dy * k, -1.0f, 1.0f) : 0.0f;
    }
    float keep = std::exp(-kRecentre * dt);
    g_sx *= keep;
    g_sy *= keep;
    float mlx = moves && !right ? g_sx : 0.0f, mly = moves && !right ? g_sy : 0.0f;
    float mrx = right ? g_sx : 0.0f, mry = right ? g_sy : 0.0f;

    // left stick: the keys steer (stick back pulls the nose up)
    float lx = mlx + float(Down(kTurnRight)) - float(Down(kTurnLeft));
    float ly = mly + float(Down(kNoseDown)) - float(Down(kNoseUp));
    p.lx = Merge(p.lx, Axis(lx));
    p.ly = Merge(p.ly, Axis(ly));

    // right stick: roll left / right, speed up / down
    float rx = mrx + float(Down(kRollRight)) - float(Down(kRollLeft));
    float ry = mry + float(Down(kSpeedUp)) - float(Down(kSpeedDown));
    p.rx = Merge(p.rx, Axis(rx));
    p.ry = Merge(p.ry, Axis(ry));

    // Immelmann: both sticks pulled back, then the right stick clicked (R3) while they are held
    static int64_t immelmann_since = -1;
    if (Down(kImmelmann)) {
        if (immelmann_since < 0) immelmann_since = now_ms;
        p.ly = Axis(-1.0f);
        p.ry = Axis(-1.0f);
        if (now_ms - immelmann_since >= 100) p.buttons |= uint16_t(1u << 7);  // the click a moment after the sticks
    } else {
        immelmann_since = -1;
    }

    // the other maneuvers: a press starts the maneuver's stick moves, which then play out on their own
    for (const Maneuver& m : kManeuvers) {
        bool d = Down(m.action);
        if (d && !was_down[m.action] && !playing) {
            playing = &m;
            step = 0;
            step_since = started = now_ms;
        }
        was_down[m.action] = d;
    }
    while (playing) {
        const Step& s = playing->steps[step];
        int64_t t = now_ms - step_since;
        if (t < s.ms || (s.hold && Down(playing->action))) {
            p.lx = Axis(s.lx);
            p.ly = Axis(s.ly);
            p.rx = Axis(s.rx);
            p.ry = Axis(s.ry);
            if (now_ms - started < 100) p.buttons |= uint16_t(1u << 7);  // R3 clicked as the sticks move
            break;
        }
        step_since = s.hold ? now_ms : step_since + s.ms;  // a held step ends now, at the key's release
        if (++step >= playing->count) playing = nullptr;
    }

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

// ImGui sets the arrow cursor again every frame unless told not to change it
void Ui() {
    ImGuiIO& io = ImGui::GetIO();
    if (g_hide) io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    else io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
}

bool MuteRumble() {
    if (g_device == kKeyboardMouse) return true;
    if (g_device == kController) return false;
    return g_kbm_used.load() > g_pad_used.load();
}

}  // namespace kbm
