// Cheats, each a setting (off by default) and a launcher row. Requested on r/recomps by Plenty_Group6674.
//
// The retail game ships its own cheats: a cheats manager (ca/res/cheats.db) runs console commands for pad codes,
// among them "god" (0x22E90) and the plane unlocks ("var g_PF_<plane> 1"). The console's commands keep their flags
// in a word at [[0x3219C8]+0x100]+0x158: 0x01 god, 0x02 all SP levels, 0x08 all SP planes, 0x20 supergun. The ones
// here use the game's own mechanisms where it has them, and add unlimited ammo and boost, which it has not:
//
// - Health: the god flag is read when a player spawns (0xA5CED; with it on, the spawn calls the unit's damage
//   object's +0xD0/+0xD4 with 100). The hook sets the flag right before that test, so every spawn and respawn
//   gets it, as if the god code had been entered.
// - Ammo and boost: each plane's per-frame boost update (0xD4250, called from the plane's update at 0xD56E7) has
//   the plane in esi. Its unit is at +0xB20; a human player's unit has a player controller at +0x7FC (an 0xC70-byte
//   object made at 0x15C3B0; null for the AI). The unit's secondary weapon is at +0x730, its ammo at weapon+0x40
//   (-1 means unlimited: the shot at 0x1B7DE7 skips the decrement), the most it holds at unit+0x724 (the loader at
//   0x155D73 reads SecondaryWeaponMaxAmmo). The plane's boost time used is at +0xC0C, the most at +0xC10
//   (MaxBoostTime, loaded at 0xD5B54); 0xD4250 adds the frame time while boosting (+0xC08 == 1) and stops at the
//   most. The hook tops up the ammo and clears the time used, for players' planes only.
// - Planes: every game variable read goes through getVar (0x43830, thiscall, name in [esp+4], value string in
//   eax at its ret 0x04 at 0x4388C). The hook answers "1" (0x292D40) for the story's plane-found flags (g_PF_*,
//   what the retail cheat codes set) and for gx_AllSpPlanesUnlocked (the console's unlockspplanes, which the
//   plane select screen reads). Not for the cheat test at 0x1E420 (all gx_All*Unlocked): when it sees one set it
//   skips the save (0x24942, 0x264E2), so it keeps seeing the real values and saving works. The save file is
//   written from the variable map itself (0x43BCC), so the unlock never reaches it: turning it off relocks.
#include "cheats.h"

#include <cstdint>
#include <cstring>

#include "core/log.h"
#include "cpu/x86_context.h"

namespace cheats {
bool g_health = false;
bool g_ammo = false;
bool g_boost = false;
bool g_planes = false;
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
    if (!cheats::g_ammo && !cheats::g_boost) return;
    uint32_t plane = ctx.esi;
    uint32_t unit = plane ? Read32(base, plane + 0xB20) : 0;
    if (!unit || !Read32(base, unit + 0x7FC)) return;  // the AI's planes have no player controller
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

// at "ret 0x04" (0x4388C) of getVar (0x43830): [esp] = the caller, [esp+4] = the name, eax = the value string
void Cheats_GetVar(X86Context& ctx, uint8_t* base) {
    if (!cheats::g_planes) return;
    uint32_t caller = Read32(base, ctx.esp);
    if (caller >= 0x1E420 && caller < 0x1E480) return;  // the cheat test that turns saving off
    uint32_t name = Read32(base, ctx.esp + 4);
    if (!name) return;
    const uint8_t* s = base + name;
    if (Match(s, "g_PF_", false) || Match(s, "gx_AllSpPlanesUnlocked", true)) {
        static int logged = 0;
        if (logged < 16) {
            logged++;
            LOGR("Cheats: %.40s read as 1 (was \"%.8s\", caller %08X)", reinterpret_cast<const char*>(s),
                 reinterpret_cast<const char*>(base + ctx.eax), caller);
        }
        ctx.eax = 0x292D40;  // "1"
    }
}
