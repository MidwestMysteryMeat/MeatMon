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
    return set;
}

std::vector<MonsterSet> teamFromJson(const nlohmann::json& array) {
    std::vector<MonsterSet> team;
    if (!array.is_array()) return team;
    for (const auto& j : array) team.push_back(monsterSetFromJson(j));
    return team;
}

} // namespace mm::battle
