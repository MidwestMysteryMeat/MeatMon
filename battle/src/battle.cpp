#include "meatmon/battle/battle.hpp"
#include "meatmon/battle/calc.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace mm::battle {

namespace {

int natureNum(const Nature* n, const char* key) {
    if (!n) return 100;
    if (n->plus == key) return 110;
    if (n->minus == key) return 90;
    return 100;
}

int* boostField(StatTable& b, const std::string& key) {
    if (key == "atk") return &b.atk;
    if (key == "def") return &b.def;
    if (key == "spa") return &b.spa;
    if (key == "spd") return &b.spd;
    if (key == "spe") return &b.spe;
    return nullptr;
}

// "hp/max" plus the status suffix Showdown-style hp strings carry.
std::string hpOf(const BattleMonster& p) {
    std::string s = std::to_string(p.hp) + "/" + std::to_string(p.stats.hp);
    if (!p.status.empty() && !p.fainted()) s += " " + p.status;
    return s;
}

// Struggle: typeless 50 BP physical, never misses, bypasses PP; recoil is
// handled in executeMove. Used automatically when a side has no PP left.
const Move kStruggle = [] {
    Move m;
    m.id = "struggle";
    m.name = "Struggle";
    m.type = "???";                 // absent from the chart => 1x vs everything
    m.category = MoveCategory::Physical;
    m.basePower = 50;
    m.accuracy = -1;
    m.pp = 1;
    return m;
}();

} // namespace

bool Side::hasReplacement() const {
    for (int i = 0; i < static_cast<int>(monsters.size()); ++i) {
        if (i != active && !monsters[i].fainted()) return true;
    }
    return false;
}

Battle::Battle(const Dex& dex, Format format, uint64_t seed, Prng::Mode rngMode)
    : dex_(dex), format_(std::move(format)), rng_(seed, rngMode) {}

void Battle::setPlayer(int side, std::string name, std::vector<MonsterSet> team) {
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

            BattleMonster p;
            p.species = sp;
            p.name = set.name.empty() ? sp->name : set.name;
            p.level = set.level;
            p.stats.hp  = calc::hpStat(sp->baseStats.hp, set.ivs.hp, set.evs.hp, set.level);
            p.stats.atk = calc::otherStat(sp->baseStats.atk, set.ivs.atk, set.evs.atk, set.level, natureNum(nat, "atk"));
            p.stats.def = calc::otherStat(sp->baseStats.def, set.ivs.def, set.evs.def, set.level, natureNum(nat, "def"));
            p.stats.spa = calc::otherStat(sp->baseStats.spa, set.ivs.spa, set.evs.spa, set.level, natureNum(nat, "spa"));
            p.stats.spd = calc::otherStat(sp->baseStats.spd, set.ivs.spd, set.evs.spd, set.level, natureNum(nat, "spd"));
            p.stats.spe = calc::otherStat(sp->baseStats.spe, set.ivs.spe, set.evs.spe, set.level, natureNum(nat, "spe"));
            p.hp = p.stats.hp;
            if (set.hp >= 0) p.hp = std::min(set.hp, p.stats.hp);
            if (!set.status.empty() && !p.fainted()) {
                p.status = set.status;
                if (p.status == "slp") p.sleepTurns = 1 + static_cast<int>(rng_.next(3));
            }

            for (const auto& mid : set.moves) {
                const Move* mv = dex_.move(mid);
                if (!mv) throw std::runtime_error("Battle: unknown move " + mid);
                p.moves.push_back({mv->id, mv->pp, mv->pp});
            }
            if (!set.ability.empty() && !dex_.ability(set.ability))
                throw std::runtime_error("Battle: unknown ability " + set.ability);
            if (!set.item.empty() && !dex_.item(set.item))
                throw std::runtime_error("Battle: unknown item " + set.item);
            p.ability = set.ability;
            p.item = set.item;
            side.monsters.push_back(std::move(p));
        }
    }

    log_.push_back("|player|p1|" + sides_[0].name);
    log_.push_back("|player|p2|" + sides_[1].name);
    log_.push_back("|gametype|" + format_.id);
    log_.push_back("|teamsize|p1|" + std::to_string(sides_[0].monsters.size()));
    log_.push_back("|teamsize|p2|" + std::to_string(sides_[1].monsters.size()));
    log_.push_back("|start");
    for (int s = 0; s < 2; ++s) {           // lead = first able monster
        int lead = -1;
        for (int i = 0; i < static_cast<int>(sides_[s].monsters.size()); ++i) {
            if (!sides_[s].monsters[i].fainted()) { lead = i; break; }
        }
        if (lead < 0) throw std::runtime_error("Battle: side has no able monster");
        switchIn(s, lead);
    }
    onSwitchInAbility(0);       // both leads are in before abilities fire
    onSwitchInAbility(1);
    beginTurn();
}

BattleMonster& Battle::active(int side) {
    return sides_[side].monsters[sides_[side].active];
}

const BattleMonster& Battle::active(int side) const {
    return sides_[side].monsters[sides_[side].active];
}

std::string Battle::tag(int side) const {
    return (side == 0 ? "p1a: " : "p2a: ") + active(side).name;
}

void Battle::switchIn(int side, int index) {
    sides_[side].active = index;
    auto& p = active(side);
    p.boosts = {};          // stages don't persist through a switch
    p.toxicN = 0;           // tox residual counter resets
    p.confusionTurns = 0;   // volatiles clear on switch
    p.flinched = false;
    std::ostringstream os;
    os << "|switch|" << tag(side) << "|" << p.species->name << ", L" << p.level
       << "|" << hpOf(p);
    log_.push_back(os.str());
}

void Battle::beginTurn() {
    ++turn_;
    log_.push_back("|turn|" + std::to_string(turn_));
    pending_[0] = pending_[1] = true;
    choices_[0] = choices_[1] = Choice{};
    for (int s = 0; s < 2; ++s) {
        if (sides_[s].active >= 0) active(s).movedThisTurn = false;
    }
    phase_ = Phase::Choices;
}

void Battle::onSwitchInAbility(int side) {
    const auto& p = active(side);
    if (p.ability.empty() || p.fainted()) return;
    const Ability* ab = dex_.ability(p.ability);
    if (!ab || ab->switchInFoeBoosts.empty()) return;
    int foe = 1 - side;
    if (sides_[foe].active < 0 || active(foe).fainted()) return;
    log_.push_back("|-ability|" + tag(side) + "|" + ab->name);
    applyBoosts(foe, ab->switchInFoeBoosts);
}

void Battle::maybeEatBerry(int side) {
    auto& p = active(side);
    if (p.fainted() || p.item.empty()) return;
    const Item* it = dex_.item(p.item);
    if (!it || it->healBelowHalf <= 0) return;
    if (p.hp * 2 >= p.stats.hp) return;
    p.hp = std::min(p.stats.hp, p.hp + it->healBelowHalf);
    log_.push_back("|-enditem|" + tag(side) + "|" + it->name + "|[eat]");
    log_.push_back("|-heal|" + tag(side) + "|" + hpOf(p) + "|[from] item: " + it->name);
    if (it->consumable) p.item.clear();
}

Request Battle::request(int side) const {
    Request req;
    if (phase_ == Phase::Choices && pending_[side]) {
        req.kind = Request::Kind::Move;
        req.moves = active(side).moves;
        bool anyPP = false;
        for (const auto& m : req.moves) anyPP |= m.pp > 0;
        if (!anyPP) req.moves = {MoveSlot{"struggle", 1, 1}};
    } else if (phase_ == Phase::FaintSwitch && needsSwitch_[side]) {
        req.kind = Request::Kind::Switch;
        const auto& s = sides_[side];
        for (int i = 0; i < static_cast<int>(s.monsters.size()); ++i) {
            if (i != s.active && !s.monsters[i].fainted()) req.switches.push_back(i);
        }
    }
    return req;
}

bool Battle::choose(int side, Choice choice) {
    if (phase_ == Phase::Choices) {
        if (choice.kind != ChoiceKind::Move || !pending_[side]) return false;
        const auto& mons = active(side).moves;
        bool anyPP = false;
        for (const auto& m : mons) anyPP |= m.pp > 0;
        if (!anyPP) {
            choices_[side] = {ChoiceKind::Move, -1};   // Struggle
            pending_[side] = false;
            return true;
        }
        if (choice.index < 0 || choice.index >= static_cast<int>(mons.size())) return false;
        if (mons[choice.index].pp <= 0) return false;
        choices_[side] = choice;
        pending_[side] = false;
        return true;
    }
    if (phase_ == Phase::FaintSwitch) {
        if (choice.kind != ChoiceKind::Switch || !needsSwitch_[side]) return false;
        auto& s = sides_[side];
        if (choice.index < 0 || choice.index >= static_cast<int>(s.monsters.size())) return false;
        if (choice.index == s.active || s.monsters[choice.index].fainted()) return false;
        switchIn(side, choice.index);
        onSwitchInAbility(side);
        needsSwitch_[side] = false;
        if (!needsSwitch_[0] && !needsSwitch_[1]) beginTurn();
        return true;
    }
    return false;
}

bool Battle::allChoicesIn() const {
    return phase_ == Phase::Choices && !pending_[0] && !pending_[1];
}

int Battle::effSpe(const BattleMonster& p) const {
    int s = calc::applyStage(p.stats.spe, p.boosts.spe);
    if (p.status == "par") s /= 2;
    return s;
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
        int idx = choices_[s].index;
        const Move* mv = idx >= 0 ? dex_.move(active(s).moves[idx].id) : &kStruggle;
        actions.push_back({s, idx, mv ? mv->priority : 0, effSpe(active(s)), rng_.next32()});
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
    endOfTurn();
    if (phase_ == Phase::Ended) return;
    if (needsSwitch_[0] || needsSwitch_[1]) {
        phase_ = Phase::FaintSwitch;
        return;
    }
    beginTurn();
}

bool Battle::beforeMove(int side) {
    auto& u = active(side);
    if (u.flinched) {
        u.flinched = false;
        log_.push_back("|cant|" + tag(side) + "|flinch");
        return false;
    }
    if (u.status == "slp") {
        if (--u.sleepTurns <= 0) {
            u.status.clear();
            log_.push_back("|-curestatus|" + tag(side) + "|slp");
        } else {
            log_.push_back("|cant|" + tag(side) + "|slp");
            return false;
        }
    }
    if (u.status == "frz") {
        if (rng_.chance(20, 100)) {
            u.status.clear();
            log_.push_back("|-curestatus|" + tag(side) + "|frz");
        } else {
            log_.push_back("|cant|" + tag(side) + "|frz");
            return false;
        }
    }
    if (u.status == "par" && rng_.chance(25, 100)) {
        log_.push_back("|cant|" + tag(side) + "|par");
        return false;
    }
    if (u.confusionTurns > 0) {
        if (--u.confusionTurns <= 0) {
            log_.push_back("|-end|" + tag(side) + "|confusion");
        } else {
            log_.push_back("|-activate|" + tag(side) + "|confusion");
            if (rng_.chance(33, 100)) {
                // Typeless 40 BP self-hit off own attack/defense; no crit,
                // no STAB, no type multipliers.
                int atk = calc::applyStage(u.stats.atk, u.boosts.atk);
                int def = calc::applyStage(u.stats.def, u.boosts.def);
                int dmg = ((2 * u.level / 5 + 2) * 40 * atk / def) / 50 + 2;
                dmg = std::max(1, dmg * rng_.damageRoll() / 100);
                u.hp = std::max(0, u.hp - dmg);
                log_.push_back("|-damage|" + tag(side) + "|" + hpOf(u) +
                               "|[from] confusion");
                checkFaint(side);
                return false;
            }
        }
    }
    return true;
}

void Battle::applyVolatile(int targetSide, const std::string& vol) {
    auto& t = active(targetSide);
    if (t.fainted()) return;
    if (vol == "confusion") {
        if (t.confusionTurns > 0) {
            log_.push_back("|-fail|" + tag(targetSide));
            return;
        }
        t.confusionTurns = 2 + static_cast<int>(rng_.next(4));   // 2-5 attempts
        log_.push_back("|-start|" + tag(targetSide) + "|confusion");
    } else if (vol == "flinch") {
        // Only meaningful if the target hasn't acted yet this turn;
        // never persists past the turn (cleared in endOfTurn).
        if (!t.movedThisTurn) t.flinched = true;
    }
}

void Battle::applyStatus(int targetSide, const std::string& status) {
    auto& t = active(targetSide);
    if (t.fainted()) return;
    auto hasType = [&](const char* ty) {
        return std::find(t.species->types.begin(), t.species->types.end(), ty) !=
               t.species->types.end();
    };
    bool immune =
        (status == "brn" && hasType("fire")) ||
        (status == "par" && hasType("electric")) ||
        ((status == "psn" || status == "tox") && (hasType("poison") || hasType("steel"))) ||
        (status == "frz" && hasType("ice"));
    if (immune) {
        log_.push_back("|-immune|" + tag(targetSide));
        return;
    }
    if (!t.status.empty()) {
        log_.push_back("|-fail|" + tag(targetSide));
        return;
    }
    t.status = status;
    if (status == "slp") t.sleepTurns = 1 + static_cast<int>(rng_.next(3));
    if (status == "tox") t.toxicN = 0;
    log_.push_back("|-status|" + tag(targetSide) + "|" + status);
}

void Battle::applyBoosts(int targetSide,
                         const std::vector<std::pair<std::string, int>>& boosts) {
    auto& t = active(targetSide);
    if (t.fainted()) return;
    for (const auto& [stat, delta] : boosts) {
        int* b = boostField(t.boosts, stat);
        if (!b) continue;
        int before = *b;
        *b = std::clamp(before + delta, -6, 6);
        int applied = *b - before;
        if (applied > 0) {
            log_.push_back("|-boost|" + tag(targetSide) + "|" + stat + "|" +
                           std::to_string(applied));
        } else if (applied < 0) {
            log_.push_back("|-unboost|" + tag(targetSide) + "|" + stat + "|" +
                           std::to_string(-applied));
        } else {
            log_.push_back("|-fail|" + tag(targetSide));
        }
    }
}

void Battle::executeMove(int atkSide, int moveIndex) {
    auto& user = active(atkSide);
    if (user.fainted()) return;
    int defSide = 1 - atkSide;
    auto& target = active(defSide);
    if (target.fainted()) return;

    user.movedThisTurn = true;
    if (!beforeMove(atkSide)) return;

    const Move* move = nullptr;
    if (moveIndex < 0) {
        move = &kStruggle;
    } else {
        auto& slot = user.moves[moveIndex];
        move = dex_.move(slot.id);
        if (!move) return;
        if (slot.pp > 0) --slot.pp;
    }

    log_.push_back("|move|" + tag(atkSide) + "|" + move->name + "|" + tag(defSide));

    if (move->accuracy > 0 &&
        !rng_.chance(static_cast<uint32_t>(move->accuracy), 100)) {
        log_.push_back("|-miss|" + tag(atkSide) + "|" + tag(defSide));
        return;
    }

    if (move->category == MoveCategory::Status) {
        int tgt = move->targetSelf ? atkSide : defSide;
        if (!move->boosts.empty()) applyBoosts(tgt, move->boosts);
        if (!move->status.empty()) applyStatus(tgt, move->status);
        if (!move->volatileStatus.empty()) applyVolatile(tgt, move->volatileStatus);
        return;
    }

    if (!target.ability.empty()) {           // Levitate-style type immunity
        const Ability* ab = dex_.ability(target.ability);
        if (ab && !ab->immuneType.empty() && ab->immuneType == move->type) {
            log_.push_back("|-immune|" + tag(defSide) + "|[from] ability: " + ab->name);
            return;
        }
    }

    double typeMult = dex_.effectiveness(move->type, target.species->types);
    if (typeMult == 0.0) {
        log_.push_back("|-immune|" + tag(defSide));
        return;
    }

    bool crit = rng_.chance(1, 24);
    bool physical = move->category == MoveCategory::Physical;
    int atkStat = physical ? calc::applyStage(user.stats.atk, user.boosts.atk)
                           : calc::applyStage(user.stats.spa, user.boosts.spa);
    int defStat = physical ? calc::applyStage(target.stats.def, target.boosts.def)
                           : calc::applyStage(target.stats.spd, target.boosts.spd);

    // Published damage formula, integer chain matching game behaviour.
    int dmg = ((2 * user.level / 5 + 2) * move->basePower * atkStat / defStat) / 50 + 2;
    if (crit) dmg = dmg * 3 / 2;
    dmg = dmg * rng_.damageRoll() / 100;
    bool stab = std::find(user.species->types.begin(), user.species->types.end(),
                          move->type) != user.species->types.end();
    if (stab) dmg = dmg * 3 / 2;
    if (!user.ability.empty()) {             // Blaze-style pinch boost
        const Ability* ab = dex_.ability(user.ability);
        if (ab && !ab->pinchBoostType.empty() && ab->pinchBoostType == move->type &&
            user.hp * 3 <= user.stats.hp) {
            dmg = dmg * 3 / 2;
        }
    }
    dmg = static_cast<int>(dmg * typeMult);
    if (physical && user.status == "brn") dmg /= 2;   // burn: 0.5x physical
    if (dmg < 1) dmg = 1;

    if (crit) log_.push_back("|-crit|" + tag(defSide));
    if (typeMult > 1.0) log_.push_back("|-supereffective|" + tag(defSide));
    else if (typeMult < 1.0) log_.push_back("|-resisted|" + tag(defSide));

    target.hp = std::max(0, target.hp - dmg);
    log_.push_back("|-damage|" + tag(defSide) + "|" + hpOf(target));

    if (!target.fainted() && move->type == "fire" && target.status == "frz") {
        target.status.clear();
        log_.push_back("|-curestatus|" + tag(defSide) + "|frz");
    }

    checkFaint(defSide);
    if (phase_ == Phase::Ended) return;

    if (!target.fainted()) {
        if (!move->secondaryStatus.empty() &&
            rng_.chance(static_cast<uint32_t>(move->secondaryChance), 100)) {
            applyStatus(defSide, move->secondaryStatus);
        }
        if (!move->secondaryVolatile.empty() &&
            rng_.chance(static_cast<uint32_t>(move->secondaryChance), 100)) {
            applyVolatile(defSide, move->secondaryVolatile);
        }
        if (move->contact && !target.ability.empty()) {   // Static-style
            const Ability* ab = dex_.ability(target.ability);
            if (ab && !ab->contactStatus.empty() &&
                rng_.chance(static_cast<uint32_t>(ab->contactStatusChance), 100)) {
                log_.push_back("|-ability|" + tag(defSide) + "|" + ab->name);
                applyStatus(atkSide, ab->contactStatus);
            }
        }
        maybeEatBerry(defSide);
    }

    if (moveIndex < 0) {                       // Struggle recoil: 1/4 max HP
        int recoil = std::max(1, user.stats.hp / 4);
        user.hp = std::max(0, user.hp - recoil);
        log_.push_back("|-damage|" + tag(atkSide) + "|" + hpOf(user) + "|[from] recoil");
        checkFaint(atkSide);
        if (phase_ != Phase::Ended && !user.fainted()) maybeEatBerry(atkSide);
    }
}

void Battle::endOfTurn() {
    int first = 0;
    int s0 = effSpe(active(0)), s1 = effSpe(active(1));
    if (s1 > s0 || (s1 == s0 && rng_.chance(1, 2))) first = 1;

    for (int k = 0; k < 2 && phase_ != Phase::Ended; ++k) {
        int side = k == 0 ? first : 1 - first;
        auto& p = active(side);
        if (p.fainted()) continue;

        int dmg = 0;
        std::string from;
        if (p.status == "brn") {
            dmg = p.stats.hp / 16;
            from = "brn";
        } else if (p.status == "psn") {
            dmg = p.stats.hp / 8;
            from = "psn";
        } else if (p.status == "tox") {
            p.toxicN = std::min(15, p.toxicN + 1);
            dmg = p.stats.hp * p.toxicN / 16;
            from = "tox";
        }
        if (!from.empty()) {
            p.hp = std::max(0, p.hp - std::max(1, dmg));
            log_.push_back("|-damage|" + tag(side) + "|" + hpOf(p) + "|[from] " + from);
            checkFaint(side);
        }
        if (phase_ == Phase::Ended || p.fainted()) continue;

        if (!p.item.empty()) {               // Leftovers-style turn heal
            const Item* it = dex_.item(p.item);
            if (it && it->healEachTurnDen > 0 && p.hp < p.stats.hp) {
                p.hp = std::min(p.stats.hp,
                                p.hp + std::max(1, p.stats.hp / it->healEachTurnDen));
                log_.push_back("|-heal|" + tag(side) + "|" + hpOf(p) +
                               "|[from] item: " + it->name);
            }
        }
        maybeEatBerry(side);
        p.flinched = false;                  // flinch never survives the turn
    }
}

void Battle::checkFaint(int defSide) {
    if (!active(defSide).fainted()) return;
    if (needsSwitch_[defSide]) return;         // already flagged this turn
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
        for (const auto& p : sides_[s].monsters) {
            nlohmann::json jp;
            jp["species"] = p.species->id;
            jp["name"] = p.name;
            jp["level"] = p.level;
            jp["hp"] = p.hp;
            jp["maxhp"] = p.stats.hp;
            jp["status"] = p.status;
            jp["ability"] = p.ability;
            jp["item"] = p.item;
            jp["confusion"] = p.confusionTurns;
            for (const auto& m : p.moves) {
                jp["moves"].push_back({{"id", m.id}, {"pp", m.pp}});
            }
            js["monsters"].push_back(std::move(jp));
        }
        j["sides"].push_back(std::move(js));
    }
    j["log"] = log_;
    return j.dump(2);
}

} // namespace mm::battle
