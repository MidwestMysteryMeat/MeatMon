#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mm::battle {

enum class MoveCategory { Physical, Special, Status };

struct StatTable {
    int hp = 0, atk = 0, def = 0, spa = 0, spd = 0, spe = 0;
};

struct Species {
    std::string id;                  // lowercase slug, e.g. "emberling"
    std::string name;                // display name
    int num = 0;                     // dex/sprite number (PokeAPI-style key)
    std::vector<std::string> types;  // 1-2 type ids
    StatTable baseStats;
};

struct Move {
    std::string id;
    std::string name;
    std::string type;
    MoveCategory category = MoveCategory::Physical;
    int basePower = 0;
    int accuracy = 100;              // -1 = never misses
    int priority = 0;
    int pp = 0;
    std::string status;              // status moves: condition to inflict
    std::string volatileStatus;      // status moves: volatile ("confusion")
    std::string secondaryStatus;     // damaging moves: chance-based rider
    std::string secondaryVolatile;   // damaging moves: volatile rider ("flinch")
    int secondaryChance = 0;         // percent
    std::vector<std::pair<std::string, int>> boosts;  // stat stage changes
    bool targetSelf = false;         // boosts/status apply to the user
    bool contact = false;            // makes contact (Static-style triggers)
};

// Abilities/items are declarative data fields interpreted by the sim.
// Behaviour too odd for a field becomes a Lua hook in Phase 4 — never a
// hardcoded per-species C++ branch.
struct Ability {
    std::string id;
    std::string name;
    std::vector<std::pair<std::string, int>> switchInFoeBoosts;  // Intimidate
    std::string immuneType;          // Levitate: full immunity to this type
    std::string pinchBoostType;      // Blaze: 1.5x this type at <=1/3 HP
    std::string contactStatus;       // Static: may status attackers on contact
    int contactStatusChance = 0;     // percent
};

struct Item {
    std::string id;
    std::string name;
    int healEachTurnDen = 0;         // Leftovers: heal max/den each turn
    int healBelowHalf = 0;           // Oran Berry: flat heal once below 1/2
    bool consumable = false;
};

struct Nature {
    std::string id;
    std::string plus;                // stat key "atk".."spe", empty = neutral
    std::string minus;
};

// Immutable battle data tables loaded from game/data/*.json.
// Hot-reload = load a fresh Dex and start new battles from it; a Dex is
// never mutated while battles reference it.
class Dex {
public:
    static Dex load(const std::filesystem::path& dataDir);

    const Species* species(const std::string& id) const;
    const Move* move(const std::string& id) const;
    const Nature* nature(const std::string& id) const;
    const Ability* ability(const std::string& id) const;
    const Item* item(const std::string& id) const;

    // Combined multiplier of attackType against a defender's type list
    // (0, 0.25, 0.5, 1, 2, 4).
    double effectiveness(const std::string& attackType,
                         const std::vector<std::string>& defenderTypes) const;

private:
    std::unordered_map<std::string, Species> species_;
    std::unordered_map<std::string, Move> moves_;
    std::unordered_map<std::string, Nature> natures_;
    std::unordered_map<std::string, Ability> abilities_;
    std::unordered_map<std::string, Item> items_;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> typeChart_;
};

} // namespace mm::battle
