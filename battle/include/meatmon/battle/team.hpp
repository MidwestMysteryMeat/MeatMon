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

// Round-trip (saves, editor, network). Carried hp/status are written only
// when set, so authored team files stay clean.
nlohmann::json monsterSetToJson(const MonsterSet& set);
nlohmann::json teamToJson(const std::vector<MonsterSet>& team);

} // namespace mm::battle
