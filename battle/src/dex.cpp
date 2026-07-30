#include "meatmon/battle/dex.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace mm::battle {

using nlohmann::json;

namespace {

json parseFile(const std::filesystem::path& file) {
    std::ifstream f(file);
    if (!f) {
        throw std::runtime_error("Dex: cannot open " + file.string());
    }
    json j = json::parse(f, nullptr, false);
    if (j.is_discarded()) {
        throw std::runtime_error("Dex: invalid JSON in " + file.string());
    }
    return j;
}

StatTable parseStats(const json& j) {
    StatTable s;
    s.hp  = j.value("hp", 0);
    s.atk = j.value("atk", 0);
    s.def = j.value("def", 0);
    s.spa = j.value("spa", 0);
    s.spd = j.value("spd", 0);
    s.spe = j.value("spe", 0);
    return s;
}

MoveCategory parseCategory(const std::string& c) {
    if (c == "special") return MoveCategory::Special;
    if (c == "status") return MoveCategory::Status;
    return MoveCategory::Physical;
}

} // namespace

Dex Dex::load(const std::filesystem::path& dataDir) {
    Dex dex;

    // Bind each parsed file to a local: items() on a temporary json dangles
    // (range-for only lifetime-extends the proxy, not the json underneath).
    const json speciesJson = parseFile(dataDir / "species.json");
    for (const auto& [id, v] : speciesJson.items()) {
        Species sp;
        sp.id = id;
        sp.name = v.value("name", id);
        sp.num = v.value("num", 0);
        sp.types = v.value("types", std::vector<std::string>{});
        sp.baseStats = parseStats(v.at("baseStats"));
        sp.catchRate = v.value("catchRate", 128);
        dex.species_.emplace(id, std::move(sp));
    }

    const json movesJson = parseFile(dataDir / "moves.json");
    for (const auto& [id, v] : movesJson.items()) {
        Move mv;
        mv.id = id;
        mv.name = v.value("name", id);
        mv.type = v.value("type", "normal");
        mv.category = parseCategory(v.value("category", "physical"));
        mv.basePower = v.value("power", 0);
        mv.accuracy = v.value("accuracy", 100);
        mv.priority = v.value("priority", 0);
        mv.pp = v.value("pp", 5);
        mv.status = v.value("status", "");
        mv.volatileStatus = v.value("volatile", "");
        if (v.contains("secondary")) {
            mv.secondaryChance = v["secondary"].value("chance", 0);
            mv.secondaryStatus = v["secondary"].value("status", "");
            mv.secondaryVolatile = v["secondary"].value("volatile", "");
        }
        if (v.contains("boosts")) {
            for (const auto& [stat, delta] : v["boosts"].items()) {
                mv.boosts.emplace_back(stat, delta.get<int>());
            }
        }
        mv.targetSelf = v.value("target", "foe") == std::string("self");
        mv.contact = v.value("contact", false);
        mv.weather = v.value("weather", "");
        dex.moves_.emplace(id, std::move(mv));
    }

    const json naturesJson = parseFile(dataDir / "natures.json");
    for (const auto& [id, v] : naturesJson.items()) {
        Nature n;
        n.id = id;
        n.plus = v.value("plus", "");
        n.minus = v.value("minus", "");
        dex.natures_.emplace(id, std::move(n));
    }

    // Optional data files: a game without abilities/items is still valid.
    if (std::filesystem::exists(dataDir / "abilities.json")) {
        const json abilitiesJson = parseFile(dataDir / "abilities.json");
        for (const auto& [id, v] : abilitiesJson.items()) {
            Ability ab;
            ab.id = id;
            ab.name = v.value("name", id);
            if (v.contains("switchInFoeBoosts")) {
                for (const auto& [stat, delta] : v["switchInFoeBoosts"].items()) {
                    ab.switchInFoeBoosts.emplace_back(stat, delta.get<int>());
                }
            }
            ab.immuneType = v.value("immuneType", "");
            ab.pinchBoostType = v.value("pinchBoostType", "");
            if (v.contains("contactStatus")) {
                ab.contactStatus = v["contactStatus"].value("status", "");
                ab.contactStatusChance = v["contactStatus"].value("chance", 0);
            }
            dex.abilities_.emplace(id, std::move(ab));
        }
    }

    if (std::filesystem::exists(dataDir / "items.json")) {
        const json itemsJson = parseFile(dataDir / "items.json");
        for (const auto& [id, v] : itemsJson.items()) {
            Item it;
            it.id = id;
            it.name = v.value("name", id);
            it.healEachTurnDen = v.value("healEachTurnDen", 0);
            it.healBelowHalf = v.value("healBelowHalf", 0);
            it.consumable = v.value("consumable", false);
            dex.items_.emplace(id, std::move(it));
        }
    }

    const json chartJson = parseFile(dataDir / "typechart.json");
    for (const auto& [atk, row] : chartJson.items()) {
        auto& dst = dex.typeChart_[atk];
        for (auto& [def, mult] : row.items()) {
            dst[def] = mult.get<double>();
        }
    }

    return dex;
}

const Species* Dex::species(const std::string& id) const {
    auto it = species_.find(id);
    return it != species_.end() ? &it->second : nullptr;
}

const Move* Dex::move(const std::string& id) const {
    auto it = moves_.find(id);
    return it != moves_.end() ? &it->second : nullptr;
}

const Nature* Dex::nature(const std::string& id) const {
    auto it = natures_.find(id);
    return it != natures_.end() ? &it->second : nullptr;
}

const Ability* Dex::ability(const std::string& id) const {
    auto it = abilities_.find(id);
    return it != abilities_.end() ? &it->second : nullptr;
}

const Item* Dex::item(const std::string& id) const {
    auto it = items_.find(id);
    return it != items_.end() ? &it->second : nullptr;
}

std::vector<const Species*> Dex::allSpecies() const {
    std::vector<const Species*> v;
    for (const auto& [id, s] : species_) v.push_back(&s);
    std::sort(v.begin(), v.end(),
              [](const Species* a, const Species* b) { return a->num < b->num; });
    return v;
}

std::vector<const Move*> Dex::allMoves() const {
    std::vector<const Move*> v;
    for (const auto& [id, m] : moves_) v.push_back(&m);
    std::sort(v.begin(), v.end(),
              [](const Move* a, const Move* b) { return a->id < b->id; });
    return v;
}

std::vector<const Ability*> Dex::allAbilities() const {
    std::vector<const Ability*> v;
    for (const auto& [id, a] : abilities_) v.push_back(&a);
    std::sort(v.begin(), v.end(),
              [](const Ability* a, const Ability* b) { return a->id < b->id; });
    return v;
}

std::vector<const Item*> Dex::allItems() const {
    std::vector<const Item*> v;
    for (const auto& [id, i] : items_) v.push_back(&i);
    std::sort(v.begin(), v.end(),
              [](const Item* a, const Item* b) { return a->id < b->id; });
    return v;
}

std::vector<const Nature*> Dex::allNatures() const {
    std::vector<const Nature*> v;
    for (const auto& [id, n] : natures_) v.push_back(&n);
    std::sort(v.begin(), v.end(),
              [](const Nature* a, const Nature* b) { return a->id < b->id; });
    return v;
}

double Dex::effectiveness(const std::string& attackType,
                          const std::vector<std::string>& defenderTypes) const {
    double mult = 1.0;
    auto row = typeChart_.find(attackType);
    if (row == typeChart_.end()) return mult;
    for (const auto& def : defenderTypes) {
        auto cell = row->second.find(def);
        if (cell != row->second.end()) mult *= cell->second;
    }
    return mult;
}

} // namespace mm::battle
