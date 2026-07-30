#pragma once
#include "battle.hpp"

#include <nlohmann/json.hpp>

#include <vector>

namespace mm::battle {

// Parse one authored set / a whole team from JSON. Used by trainer entities
// on maps, the player save, the team builder, and network payloads:
//   { "species": "emberling", "level": 50, "nature": "adamant",
//     "ability": "blaze", "item": "leftovers",
//     "moves": ["ember", "tackle"],
//     "evs": {"atk": 252, "spe": 252}, "ivs": {"spa": 0} }
MonsterSet monsterSetFromJson(const nlohmann::json& j);
std::vector<MonsterSet> teamFromJson(const nlohmann::json& array);

// Round-trip (saves, editor, network). Carried hp/status/exp are written
// only when set, so authored team files stay clean.
nlohmann::json monsterSetToJson(const MonsterSet& set);
nlohmann::json teamToJson(const std::vector<MonsterSet>& team);

// Progression (RPG overworld layer, not the battle sim itself — computed
// after a battle ends and applied to the surviving MonsterSet).
//
// Cumulative EXP required to BE at `level` (cubic curve, capped at 100).
long long expForLevel(int level);
// Adds `gained` to set.exp, leveling up in place while thresholds are
// crossed. No-op past level 100. Returns levels gained (0 if none).
int gainExp(MonsterSet& set, int gained);
// EXP awarded for defeating a monster of `foeSpecies` at `foeLevel`.
int expYieldFor(const Species& foeSpecies, int foeLevel);

} // namespace mm::battle
