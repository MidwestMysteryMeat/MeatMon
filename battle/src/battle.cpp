#include "meatmon/battle/battle.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace mm::battle {

namespace {

// Standard published stat formulas (integer math throughout).
int hpStat(int base, int iv, int ev, int level) {
    return (2 * base + iv + ev / 4) * level / 100 + level + 10;
}

// natureNum: 110 boosted, 90 hindered, 100 neutral.
int otherStat(int base, int iv, int ev, int level, int natureNum) {
    int s = (2 * base + iv + ev / 4) * level / 100 + 5;
    return s * natureNum / 100;
}

int natureNum(const Nature* n, const char* key) {
    if (!n) return 100;
    if (n->plus == key) return 110;
    if (n->minus == key) return 90;
    return 100;
}

} // namespace

bool Side::hasReplacement() const {
    for (int i = 0; i < static_cast<int>(pokemon.size()); ++i) {
        if (i != active && !pokemon[i].fainted()) return true;
    }
    return false;
}

Battle::Battle(const Dex& dex, Format format, uint64_t seed, Prng::Mode rngMode)
    : dex_(dex), format_(std::move(format)), rng_(seed, rngMode) {}

void Battle::setPlayer(int side, std::string name, std::vector<PokemonSet> team) {
    sides_[side].name = std::move(name);
    sides_[side].team = std::move(team);
}

void Battle::start() {
    if (phase_ != Phase::Setup) return;

    for (auto& side : sides_) {
        if (side.team.empty()) throw std::runtime_error("Battle: empty team");
        for (const auto& set : side.team) {
            const Species* sp = dex_.species(set.species);
            if (!sp) throw std::runtime_error("Battle: unknown species " + set.species);
            const Nature* nat = dex_.nature(set.nature);

            BattlePokemon p;
            p.species = sp;
            p.name = set.name.empty() ? sp->name : set.name;
            p.level = set.level;
            p.stats.hp  = hpStat(sp->baseStats.hp, set.ivs.hp, set.evs.hp, set.level);
            p.stats.atk = otherStat(sp->baseStats.atk, set.ivs.atk, set.evs.atk, set.level, natureNum(nat, "atk"));
            p.stats.def = otherStat(sp->baseStats.def, set.ivs.def, set.evs.def, set.level, natureNum(nat, "def"));
            p.stats.spa = otherStat(sp->baseStats.spa, set.ivs.spa, set.evs.spa, set.level, natureNum(nat, "spa"));
            p.stats.spd = otherStat(sp->baseStats.spd, set.ivs.spd, set.evs.spd, set.level, natureNum(nat, "spd"));
            p.stats.spe = otherStat(sp->baseStats.spe, set.ivs.spe, set.evs.spe, set.level, natureNum(nat, "spe"));
            p.hp = p.stats.hp;

            for (const auto& mid : set.moves) {
                const Move* mv = dex_.move(mid);
                if (!mv) throw std::runtime_error("Battle: unknown move " + mid);
                p.moves.push_back({mv->id, mv->pp, mv->pp});
            }
            side.pokemon.push_back(std::move(p));
        }
    }

    log_.push_back("|player|p1|" + sides_[0].name);
    log_.push_back("|player|p2|" + sides_[1].name);
    log_.push_back("|gametype|" + format_.id);
    log_.push_back("|teamsize|p1|" + std::to_string(sides_[0].pokemon.size()));
    log_.push_back("|teamsize|p2|" + std::to_string(sides_[1].pokemon.size()));
    log_.push_back("|start");
    switchIn(0, 0);
    switchIn(1, 0);
    beginTurn();
}

BattlePokemon& Battle::active(int side) {
    return sides_[side].pokemon[sides_[side].active];
}

const BattlePokemon& Battle::active(int side) const {
    return sides_[side].pokemon[sides_[side].active];
}

std::string Battle::tag(int side) const {
    return (side == 0 ? "p1a: " : "p2a: ") + active(side).name;
}

void Battle::switchIn(int side, int index) {
    sides_[side].active = index;
    const auto& p = active(side);
    std::ostringstream os;
    os << "|switch|" << tag(side) << "|" << p.species->name << ", L" << p.level
       << "|" << p.hp << "/" << p.stats.hp;
    log_.push_back(os.str());
}

void Battle::beginTurn() {
    ++turn_;
    log_.push_back("|turn|" + std::to_string(turn_));
    pending_[0] = pending_[1] = true;
    choices_[0] = choices_[1] = Choice{};
    phase_ = Phase::Choices;
}

Request Battle::request(int side) const {
    Request req;
    if (phase_ == Phase::Choices && pending_[side]) {
        req.kind = Request::Kind::Move;
        req.moves = active(side).moves;
    } else if (phase_ == Phase::FaintSwitch && needsSwitch_[side]) {
        req.kind = Request::Kind::Switch;
        const auto& s = sides_[side];
        for (int i = 0; i < static_cast<int>(s.pokemon.size()); ++i) {
            if (i != s.active && !s.pokemon[i].fainted()) req.switches.push_back(i);
        }
    }
    return req;
}

bool Battle::choose(int side, Choice choice) {
    if (phase_ == Phase::Choices) {
        if (choice.kind != ChoiceKind::Move || !pending_[side]) return false;
        const auto& mons = active(side).moves;
        if (choice.index < 0 || choice.index >= static_cast<int>(mons.size())) return false;
        if (mons[choice.index].pp <= 0) return false;   // Struggle is Phase 3
        choices_[side] = choice;
        pending_[side] = false;
        return true;
    }
    if (phase_ == Phase::FaintSwitch) {
        if (choice.kind != ChoiceKind::Switch || !needsSwitch_[side]) return false;
        auto& s = sides_[side];
        if (choice.index < 0 || choice.index >= static_cast<int>(s.pokemon.size())) return false;
        if (choice.index == s.active || s.pokemon[choice.index].fainted()) return false;
        switchIn(side, choice.index);
        needsSwitch_[side] = false;
        if (!needsSwitch_[0] && !needsSwitch_[1]) beginTurn();
        return true;
    }
    return false;
}

bool Battle::allChoicesIn() const {
    return phase_ == Phase::Choices && !pending_[0] && !pending_[1];
}

void Battle::commitTurn() {
    if (!allChoicesIn()) return;

    struct Action {
        int side;
        int move;
        int prio;
        int speed;
        uint32_t tie;
    };
    std::vector<Action> actions;
    for (int s = 0; s < 2; ++s) {
        const Move* mv = dex_.move(active(s).moves[choices_[s].index].id);
        actions.push_back({s, choices_[s].index, mv ? mv->priority : 0,
                           active(s).stats.spe, rng_.next32()});
    }
    std::sort(actions.begin(), actions.end(), [](const Action& a, const Action& b) {
        if (a.prio != b.prio) return a.prio > b.prio;
        if (a.speed != b.speed) return a.speed > b.speed;
        return a.tie > b.tie;
    });

    for (const auto& a : actions) {
        if (phase_ == Phase::Ended) break;
        executeMove(a.side, a.move);
    }

    if (phase_ == Phase::Ended) return;
    if (needsSwitch_[0] || needsSwitch_[1]) {
        phase_ = Phase::FaintSwitch;
        return;
    }
    beginTurn();
}

void Battle::executeMove(int atkSide, int moveIndex) {
    auto& user = active(atkSide);
    if (user.fainted()) return;

    int defSide = 1 - atkSide;
    auto& target = active(defSide);
    if (target.fainted()) return;

    auto& slot = user.moves[moveIndex];
    const Move* move = dex_.move(slot.id);
    if (!move) return;
    if (slot.pp > 0) --slot.pp;

    log_.push_back("|move|" + tag(atkSide) + "|" + move->name + "|" + tag(defSide));

    if (move->accuracy > 0 &&
        !rng_.chance(static_cast<uint32_t>(move->accuracy), 100)) {
        log_.push_back("|-miss|" + tag(atkSide) + "|" + tag(defSide));
        return;
    }

    if (move->category == MoveCategory::Status || move->basePower <= 0) {
        // Status effects land in Phase 3; the scaffold logs the move only.
        return;
    }

    double typeMult = dex_.effectiveness(move->type, target.species->types);
    if (typeMult == 0.0) {
        log_.push_back("|-immune|" + tag(defSide));
        return;
    }

    bool crit = rng_.chance(1, 24);
    bool physical = move->category == MoveCategory::Physical;
    int atkStat = physical ? user.stats.atk : user.stats.spa;
    int defStat = physical ? target.stats.def : target.stats.spd;

    // Published damage formula, integer chain matching game behaviour.
    int dmg = ((2 * user.level / 5 + 2) * move->basePower * atkStat / defStat) / 50 + 2;
    if (crit) dmg = dmg * 3 / 2;
    dmg = dmg * rng_.damageRoll() / 100;
    bool stab = std::find(user.species->types.begin(), user.species->types.end(),
                          move->type) != user.species->types.end();
    if (stab) dmg = dmg * 3 / 2;
    dmg = static_cast<int>(dmg * typeMult);
    if (dmg < 1) dmg = 1;

    if (crit) log_.push_back("|-crit|" + tag(defSide));
    if (typeMult > 1.0) log_.push_back("|-supereffective|" + tag(defSide));
    else if (typeMult < 1.0) log_.push_back("|-resisted|" + tag(defSide));

    target.hp = std::max(0, target.hp - dmg);
    log_.push_back("|-damage|" + tag(defSide) + "|" + std::to_string(target.hp) +
                   "/" + std::to_string(target.stats.hp));
    checkFaint(defSide);
}

void Battle::checkFaint(int defSide) {
    if (!active(defSide).fainted()) return;
    log_.push_back("|faint|" + tag(defSide));
    if (sides_[defSide].hasReplacement()) {
        needsSwitch_[defSide] = true;
    } else {
        winner_ = 1 - defSide;
        phase_ = Phase::Ended;
        log_.push_back("|win|" + sides_[winner_].name);
    }
}

std::string Battle::serialize() const {
    nlohmann::json j;
    j["turn"] = turn_;
    j["phase"] = static_cast<int>(phase_);
    j["winner"] = winner_;
    j["rngState"] = rng_.state();
    j["format"] = format_.id;
    for (int s = 0; s < 2; ++s) {
        nlohmann::json js;
        js["name"] = sides_[s].name;
        js["active"] = sides_[s].active;
        for (const auto& p : sides_[s].pokemon) {
            nlohmann::json jp;
            jp["species"] = p.species->id;
            jp["name"] = p.name;
            jp["level"] = p.level;
            jp["hp"] = p.hp;
            jp["maxhp"] = p.stats.hp;
            for (const auto& m : p.moves) {
                jp["moves"].push_back({{"id", m.id}, {"pp", m.pp}});
            }
            js["pokemon"].push_back(std::move(jp));
        }
        j["sides"].push_back(std::move(js));
    }
    j["log"] = log_;
    return j.dump(2);
}

} // namespace mm::battle
