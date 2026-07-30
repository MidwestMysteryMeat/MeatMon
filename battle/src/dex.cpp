#include "meatmon/battle/dex.hpp"

#include <nlohmann/json.hpp>

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
        if (v.contains("secondary")) {
            mv.secondaryChance = v["secondary"].value("chance", 0);
            mv.secondaryStatus = v["secondary"].value("status", "");
        }
        if (v.contains("boosts")) {
            for (const auto& [stat, delta] : v["boosts"].items()) {
                mv.boosts.emplace_back(stat, delta.get<int>());
            }
        }
        mv.targetSelf = v.value("target", "foe") == std::string("self");
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
