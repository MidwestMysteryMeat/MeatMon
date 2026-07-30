#include "meatmon/battle/team.hpp"

namespace mm::battle {

namespace {

StatTable statsFrom(const nlohmann::json& j, StatTable base) {
    base.hp  = j.value("hp", base.hp);
    base.atk = j.value("atk", base.atk);
    base.def = j.value("def", base.def);
    base.spa = j.value("spa", base.spa);
    base.spd = j.value("spd", base.spd);
    base.spe = j.value("spe", base.spe);
    return base;
}

} // namespace

MonsterSet monsterSetFromJson(const nlohmann::json& j) {
    MonsterSet set;
    set.species = j.value("species", "");
    set.name = j.value("name", "");
    set.level = j.value("level", 50);
    set.nature = j.value("nature", "hardy");
    set.ability = j.value("ability", "");
    set.item = j.value("item", "");
    set.moves = j.value("moves", std::vector<std::string>{});
    if (j.contains("evs")) set.evs = statsFrom(j["evs"], StatTable{});
    if (j.contains("ivs")) set.ivs = statsFrom(j["ivs"], set.ivs);
    set.hp = j.value("hp", -1);
    set.status = j.value("status", "");
    return set;
}

std::vector<MonsterSet> teamFromJson(const nlohmann::json& array) {
    std::vector<MonsterSet> team;
    if (!array.is_array()) return team;
    for (const auto& j : array) team.push_back(monsterSetFromJson(j));
    return team;
}

nlohmann::json monsterSetToJson(const MonsterSet& set) {
    nlohmann::json j;
    j["species"] = set.species;
    if (!set.name.empty()) j["name"] = set.name;
    j["level"] = set.level;
    j["nature"] = set.nature;
    if (!set.ability.empty()) j["ability"] = set.ability;
    if (!set.item.empty()) j["item"] = set.item;
    j["moves"] = set.moves;
    auto stats = [](const StatTable& s) {
        return nlohmann::json{{"hp", s.hp}, {"atk", s.atk}, {"def", s.def},
                              {"spa", s.spa}, {"spd", s.spd}, {"spe", s.spe}};
    };
    j["evs"] = stats(set.evs);
    j["ivs"] = stats(set.ivs);
    if (set.hp >= 0) j["hp"] = set.hp;
    if (!set.status.empty()) j["status"] = set.status;
    return j;
}

nlohmann::json teamToJson(const std::vector<MonsterSet>& team) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : team) arr.push_back(monsterSetToJson(s));
    return arr;
}

} // namespace mm::battle
