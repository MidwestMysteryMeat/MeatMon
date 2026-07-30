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

} // namespace mm::battle
