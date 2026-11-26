// Cheats, each a setting (off by default) and a launcher row. Requested on r/recomps by Plenty_Group6674.
//
// The retail game ships its own cheats: a cheats manager (ca/res/cheats.db) runs console commands for pad codes,
// among them "god" (0x22E90) and the plane unlocks ("var g_PF_<plane> 1"). The console's commands keep their flags
// in a word at [[0x3219C8]+0x100]+0x158: 0x01 god, 0x02 all SP levels, 0x08 all SP planes, 0x20 supergun. The ones
// here use the game's own mechanisms where it has them, and add unlimited ammo and boost, which it has not:
//
// - Health: the god flag is read when a player spawns (0xA5CED; with it on, the spawn calls the unit's damage
//   object's +0xD0/+0xD4 with 100). The hook sets the flag right before that test, so every spawn and respawn
//   gets it, as if the god code had been entered. Those calls are SetHealth (unit vtable +0xD0, 0x95280: health at
//   unit+0x160, clamped to min +0x164 and max +0x168) and SetMinHealth (+0xD4, 0x16D370), both with 100, so the
//   health stayed at 100 of a much higher max; the plane hook below keeps min and health at the max instead.
// - Ammo and boost: each plane's per-frame boost update (0xD4250, called from the plane's update at 0xD56E7) has
//   the plane in esi. Its unit is at +0xB20; a human player's unit has a player controller at +0x7FC (an 0xC70-byte
//   object made at 0x15C3B0; null for the AI). The unit's secondary weapon is at +0x730, its ammo at weapon+0x40
//   (-1 means unlimited: the shot at 0x1B7DE7 skips the decrement), the most it holds at unit+0x724 (the loader at
//   0x155D73 reads SecondaryWeaponMaxAmmo). The plane's boost time used is at +0xC0C, the most at +0xC10
//   (MaxBoostTime, loaded at 0xD5B54); 0xD4250 adds the frame time while boosting (+0xC08 == 1) and stops at the
//   most. The hook tops up the ammo and clears the time used, for players' planes only.
// - Planes: every game variable read goes through getVar (0x43830, thiscall, name in [esp+4], value string in
//   eax at its ret 0x04 at 0x4388C). The hook answers "1" (0x292D40) for the story's plane-found flags (g_PF_*,
//   what the retail cheat codes set; 0 locked, 1 found, 2 upgraded, so only a 0 or missing value is raised and
//   upgrades still work; an upgrade bought writes 2 and so does reach the save) and for gx_AllSpPlanesUnlocked (the console's unlockspplanes, which the
//   plane select screen reads). Not for the cheat test at 0x1E420 (all gx_All*Unlocked): when it sees one set it
//   skips the save (0x24942, 0x264E2), so it keeps seeing the real values and saving works. The save file is
//   written from the variable map itself (0x43BCC), so the unlock never reaches it: turning it off relocks.
// - Money and tokens: the hangar's money is g_Player0Score and its upgrade tokens g_Player0UpgradeTokens (the
//   retail codes "addvar" 5000 and 10 to them). getVar answers the game's own caps for them, "999999" (0x293B74)
//   and "999" (0x293B70), the most setVar (0x43890: thiscall, name in [esp+4], value string in [esp+8], ret 0x08)
//   ever stores. A hook at setVar's entry drops the writes made from those answers: the ones whose caller is in
//   the function (within 0x200 bytes after the call) that recently read the cap of that variable (the last 8
//   such readers are kept). That is how
//   spending is done: the hangar reads both at 0x1056C4/0x1056E1, subtracts the cost and writes them at
//   0x10570F/0x10572B; the console's addvar (0x21760, also what the scripts' "add to variable" runs) reads at
//   0x2179B and writes at 0x217E5. Every other write goes through, above all the loaders, which set each stored
//   value without reading it first (the profile's "name=value" lines at 0x44175, the records at 0x7A1C0): a
//   cheat that drops those leaves 0 behind the cap, and the game saves the profile on its own (it did, on load).
//   The variable manager's own code (0x43000-0x45000: defaults, carry-over copies) also reads the real amounts.
//   So buying, repairing and gambling cost nothing and the save keeps the real amounts; what is earned while
//   the cheat is on is not kept.
#include "cheats.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "core/log.h"
#include "cpu/x86_context.h"
#include "kbm.h"

namespace cheats {
bool g_health = false;
bool g_ammo = false;
bool g_boost = false;
bool g_planes = false;
bool g_money = false;
bool g_tokens = false;
}  // namespace cheats

namespace {
uint32_t Read32(uint8_t* base, uint32_t a) {
    uint32_t v;
    std::memcpy(&v, base + a, 4);
    return v;
}
void Write32(uint8_t* base, uint32_t a, uint32_t v) { std::memcpy(base + a, &v, 4); }

// a guest string starts with (or, whole, equals) the ASCII text t, ignoring case as the game's variables do
bool Match(const uint8_t* s, const char* t, bool whole) {
    for (; *t; s++, t++) {
        uint8_t a = *s, b = uint8_t(*t);
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
    }
    return !whole || *s == 0;
}
}  // namespace

// before "test byte ptr [edx+0x158], 0x01" at 0xA5CED in a player's spawn: edx = the console's flags object
void Cheats_God(X86Context& ctx, uint8_t* base) {
    if (!cheats::g_health || !ctx.edx) return;
    uint32_t flags = Read32(base, ctx.edx + 0x158);
    if (!(flags & 1)) {
        Write32(base, ctx.edx + 0x158, flags | 1);
        static bool logged = false;
        if (!logged) LOGR("Cheats: unlimited health (the game's god cheat) on");
        logged = true;
    }
}

// at 0xD4253 in the plane's boost update (0xD4250), after "mov esi, ecx": esi = the plane
void Cheats_Plane(X86Context& ctx, uint8_t* base) {
    uint32_t plane = ctx.esi;
    uint32_t unit = plane ? Read32(base, plane + 0xB20) : 0;
    if (!unit || !Read32(base, unit + 0x7FC)) return;  // the AI's planes have no player controller
    // the maneuver playing (+0xD68, index into the entries at +0xCE0, -1 none): in the first move's last
    // ManeuverComboTime (0.5 s) the game re-checks the sticks every frame until the move ends (0xD501F), and a
    // released stick swaps the queued follow-up ("RollRight_Right") for the plain finish ("RollRight_Default"). So the
    // hold keys wait while a first move (an entry name with no '_', which follow-ups have) plays.
    {
        uint32_t i = Read32(base, plane + 0xD68), entry = 0;
        if (int32_t(i) >= 0 && i < 64) entry = Read32(base, plane + 0xCE0 + i * 4);
        bool first = false;
        if (entry) {
            first = true;
            for (const uint8_t* s = base + entry; s < base + entry + 32 && *s; s++)  // the name is the entry's start
                if (*s == '_') first = false;
        }
        kbm::PlaneManeuver(first);
    }
    if (!cheats::g_ammo && !cheats::g_boost && !cheats::g_health) return;
    static uint32_t logged = 0;
    if (logged != plane) {  // once per player plane (a new one each spawn)
        logged = plane;
        uint32_t weapon = Read32(base, unit + 0x730);
        float used, most;
        std::memcpy(&used, base + plane + 0xC0C, 4);
        std::memcpy(&most, base + plane + 0xC10, 4);
        LOGR("Cheats: player plane %08X unit %08X: secondary ammo %d of %d, boost used %.2f of %.2f s", plane, unit,
             weapon ? int32_t(Read32(base, weapon + 0x40)) : -2, int32_t(Read32(base, unit + 0x724)), used, most);
    }
    if (cheats::g_health) {
        // the god cheat pins health at 100 (SetHealth then SetMinHealth with 100), but planes' max is far higher, so
        // the HUD showed about 1%: keep the minimum and the health at the max instead (reported on r/recomps by
        // Plenty_Group6674). Fields written directly: the setters would send a network event each frame.
        uint32_t most = Read32(base, unit + 0x168);
        if (int32_t(most) > 0) {
            if (Read32(base, unit + 0x164) != most) Write32(base, unit + 0x164, most);
            if (Read32(base, unit + 0x160) != most) Write32(base, unit + 0x160, most);
        }
    }
    if (cheats::g_boost) {
        float zero = 0;
        std::memcpy(base + plane + 0xC0C, &zero, 4);
    }
    if (cheats::g_ammo) {
        uint32_t weapon = Read32(base, unit + 0x730);
        int32_t most = int32_t(Read32(base, unit + 0x724));
        if (weapon && most > 0) {
            int32_t ammo = int32_t(Read32(base, weapon + 0x40));
            if (ammo != -1 && ammo < most) Write32(base, weapon + 0x40, uint32_t(most));
        }
    }
}

// the variable manager's own code (load, defaults, carry-over): it reads the real money and tokens
// per variable (1 money, 2 tokens): the return addresses of the last getVars that answered the cap (0 none), so a
// read elsewhere between a spending read and its write doesn't hide it
constexpr int kReaders = 8;
static uint32_t g_cap_reader[3][kReaders];
static int g_cap_next[3];

static bool Manager(uint32_t caller) { return caller >= 0x43000 && caller < 0x45000; }

// the variable a money or tokens cheat stands in for: 1 money, 2 tokens, 0 neither (or that cheat is off)
static int MoneyVar(const uint8_t* s) {
    if (cheats::g_money && Match(s, "g_Player0Score", true)) return 1;
    if (cheats::g_tokens && Match(s, "g_Player0UpgradeTokens", true)) return 2;
    return 0;
}

// at "ret 0x04" (0x4388C) of getVar (0x43830): [esp] = the caller, [esp+4] = the name, eax = the value string
void Cheats_GetVar(X86Context& ctx, uint8_t* base) {
    if (!cheats::g_planes && !cheats::g_money && !cheats::g_tokens) return;
    uint32_t caller = Read32(base, ctx.esp);
    uint32_t name = Read32(base, ctx.esp + 4);
    if (!name) return;
    const uint8_t* s = base + name;
    if (int m = MoneyVar(s); m && !Manager(caller)) {
        static bool logged[3];
        if (!logged[m]) {
            logged[m] = true;
            LOGR("Cheats: %s read as %s (was \"%.12s\", caller %08X)", m == 1 ? "money" : "tokens",
                 m == 1 ? "999999" : "999", reinterpret_cast<const char*>(base + ctx.eax), caller);
        }
        ctx.eax = m == 1 ? 0x293B74 : 0x293B70;  // "999999", "999": the caps setVar stores at most
        uint32_t* r = g_cap_reader[m];
        if (std::find(r, r + kReaders, caller) == r + kReaders) {  // newest replaces the oldest
            r[g_cap_next[m]] = caller;
            g_cap_next[m] = (g_cap_next[m] + 1) % kReaders;
        }
        return;
    }
    if (!cheats::g_planes) return;
    if (caller >= 0x1E420 && caller < 0x1E480) return;  // the cheat test that turns saving off
    if (Match(s, "g_PF_", false) || Match(s, "gx_AllSpPlanesUnlocked", true)) {
        // g_PF_* is 0 locked, 1 found, 2 upgraded (the hangar writes 2): raise only a locked or missing value, so
        // upgrades still show and can be bought (reported on r/recomps by Plenty_Group6674)
        if (ctx.eax) {
            const uint8_t* v = base + ctx.eax;
            if (v[0] && !(v[0] == '0' && v[1] == 0)) return;
        }
        static int logged = 0;
        if (logged < 16) {
            logged++;
            LOGR("Cheats: %.40s read as 1 (was \"%.8s\", caller %08X)", reinterpret_cast<const char*>(s),
                 reinterpret_cast<const char*>(base + ctx.eax), caller);
        }
        ctx.eax = 0x292D40;  // "1"
    }
}

// at setVar's entry (0x43890): [esp] = the caller, [esp+4] = the name, [esp+8] = the value string; true returns
// without the write
bool Cheats_SetVar(X86Context& ctx, uint8_t* base) {
    if (!cheats::g_money && !cheats::g_tokens) return false;
    uint32_t name = Read32(base, ctx.esp + 4), value = Read32(base, ctx.esp + 8);
    if (!name || !value) return false;
    int m = MoneyVar(base + name);
    if (!m) return false;
    uint32_t caller = Read32(base, ctx.esp), reader = 0;
    for (uint32_t r : g_cap_reader[m])  // the same function read the cap just before: the nearest such read
        if (r && caller > r && caller - r < 0x200 && caller - r < caller - reader) reader = r;
    bool from_cap = reader != 0;
    static int logged = 0;
    if (logged < 16) {
        logged++;
        LOGR("Cheats: %s write \"%.12s\" %s (caller %08X, cap read at %08X)", m == 1 ? "money" : "tokens",
             reinterpret_cast<const char*>(base + value), from_cap ? "dropped" : "kept", caller, reader);
    }
    return from_cap;
}
