#pragma once

#include <cstdint>

namespace cheats {
extern bool g_health;  // cheat_health: the game's own god cheat on every player at spawn
extern bool g_ammo;    // cheat_ammo: the players' secondary weapons stay full
extern bool g_boost;   // cheat_boost: the players' boost never runs out
extern bool g_planes;  // cheat_planes: every plane counts as found (hangar) and the plane select shows all of them
extern bool g_money;   // cheat_money: the hangar shows 999,999 money and buying costs nothing (not saved)
extern bool g_tokens;  // cheat_tokens: 999 upgrade tokens and upgrades cost none (not saved)
extern bool g_keys;    // cheat_keys: F5-F10 turn the cheats on and off in game (on by default)

// GameInfo::message: F5 health, F6 ammo, F7 boost, F8 planes, F9 money, F10 tokens, each with a note on screen
bool Message(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
}  // namespace cheats
