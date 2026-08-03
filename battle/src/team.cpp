#include "meatmon/battle/team.hpp"

#include <algorithm>

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
    set.exp = j.value("exp", 0);
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
    if (set.exp > 0) j["exp"] = set.exp;
    return j;
}

nlohmann::json teamToJson(const std::vector<MonsterSet>& team) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : team) arr.push_back(monsterSetToJson(s));
    return arr;
}

long long expForLevel(int level) {
    if (level <= 1) return 0;
    long long l = level;
    return l * l * l;
}

int gainExp(MonsterSet& set, int gained) {
    if (set.level >= 100 || gained <= 0) return 0;
    // Mons authored above level 1 ship with exp 0; seed the cumulative total
    // for their current level so the next level is one delta away, not the
    // full cubic total from level 1.
    long long baseline = expForLevel(set.level);
    if (set.exp < baseline) set.exp = static_cast<int>(baseline);
    set.exp += gained;
    int levels = 0;
    while (set.level < 100 && set.exp >= expForLevel(set.level + 1)) {
        ++set.level;
        ++levels;
    }
    return levels;
}

int expYieldFor(const Species& foeSpecies, int foeLevel) {
    return std::max(1, foeSpecies.baseExpYield * foeLevel / 7);
}

} // namespace mm::battle
