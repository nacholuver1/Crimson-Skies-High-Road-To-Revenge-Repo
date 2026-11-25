#pragma once

namespace cheats {
extern bool g_health;  // cheat_health: the game's own god cheat on every player at spawn
extern bool g_ammo;    // cheat_ammo: the players' secondary weapons stay full
extern bool g_boost;   // cheat_boost: the players' boost never runs out
extern bool g_planes;  // cheat_planes: every plane counts as found (hangar) and the plane select shows all of them
extern bool g_money;   // cheat_money: the hangar shows 999,999 money and buying costs nothing (not saved)
extern bool g_tokens;  // cheat_tokens: 999 upgrade tokens and upgrades cost none (not saved)
}  // namespace cheats
