#pragma once
#include "dex.hpp"
#include "rng.hpp"

#include <array>
#include <string>
#include <vector>

namespace mm::battle {

// A team member as authored (team builder / script / network payload).
struct PokemonSet {
    std::string name;                        // nickname; empty = species name
    std::string species;                     // species id
    int level = 50;
    std::string nature = "hardy";
    StatTable evs{};
    StatTable ivs{31, 31, 31, 31, 31, 31};
    std::vector<std::string> moves;          // move ids
};

struct MoveSlot {
    std::string id;
    int pp = 0;
    int maxpp = 0;
};

// In-battle state. stats.hp is max HP.
struct BattlePokemon {
    const Species* species = nullptr;
    std::string name;
    int level = 50;
    StatTable stats{};
    int hp = 0;
    std::vector<MoveSlot> moves;
    std::string status;              // "", brn, par, psn, tox, slp, frz
    int sleepTurns = 0;              // remaining slp turns
    int toxicN = 0;                  // tox residual counter (resets on switch)
    StatTable boosts{};              // stat stages -6..+6 (hp unused)

    bool fainted() const { return hp <= 0; }
};

struct Side {
    std::string name;
    std::vector<PokemonSet> team;            // as submitted
    std::vector<BattlePokemon> pokemon;      // built at start()
    int active = -1;                         // index into pokemon (singles)

    bool hasReplacement() const;
};

enum class ChoiceKind { Move, Switch };

struct Choice {
    ChoiceKind kind = ChoiceKind::Move;
    int index = 0;                           // move slot / party index
                                             // (index -1 = Struggle, set by
                                             // the sim when no PP remains)
};

// What a side may legally do right now. Mirrors the shape of Showdown's
// request protocol: the sim pushes a request, the client answers a choice.
struct Request {
    enum class Kind { Wait, Move, Switch };
    Kind kind = Kind::Wait;
    std::vector<MoveSlot> moves;             // Kind::Move: legal move slots
    std::vector<int> switches;               // Kind::Switch: legal party idxs
};

struct Format {
    std::string id = "singles";
    int activePerSide = 1;                   // doubles/triples land in Phase 3
};

// One authoritative battle. Deterministic: same seed + same choices produce
// a byte-identical protocol log. Observers (UI, spectators, replays) consume
// log(); they never inspect sim internals.
class Battle {
public:
    Battle(const Dex& dex, Format format, uint64_t seed,
           Prng::Mode rngMode = Prng::Mode::Showdown);

    void setPlayer(int side, std::string name, std::vector<PokemonSet> team);
    void start();

    Request request(int side) const;
    bool choose(int side, Choice choice);    // false = illegal, state unchanged
    bool allChoicesIn() const;
    void commitTurn();                       // resolves turn, extends log

    bool ended() const { return phase_ == Phase::Ended; }
    int winner() const { return winner_; }   // -1 until ended
    int turn() const { return turn_; }
    const std::vector<std::string>& log() const { return log_; }

    // JSON snapshot (turn, sides, HP/PP, RNG state, log) for network sync
    // and replays. Full restore round-trip is Phase 3 (see ROADMAP).
    std::string serialize() const;

private:
    enum class Phase { Setup, Choices, FaintSwitch, Ended };

    BattlePokemon& active(int side);
    const BattlePokemon& active(int side) const;
    std::string tag(int side) const;         // "p1a: Nickname"
    void switchIn(int side, int index);
    void beginTurn();
    void executeMove(int side, int moveIndex);   // moveIndex -1 = Struggle
    void checkFaint(int defSide);
    bool beforeMove(int side);               // slp/frz/par gates, logs |cant|
    void applyStatus(int targetSide, const std::string& status);
    void applyBoosts(int targetSide,
                     const std::vector<std::pair<std::string, int>>& boosts);
    void endOfTurn();                        // residual brn/psn/tox damage
    int effSpe(const BattlePokemon& p) const;

    const Dex& dex_;
    Format format_;
    Prng rng_;
    std::array<Side, 2> sides_{};
    std::array<bool, 2> pending_{false, false};
    std::array<Choice, 2> choices_{};
    std::array<bool, 2> needsSwitch_{false, false};
    Phase phase_ = Phase::Setup;
    int turn_ = 0;
    int winner_ = -1;
    std::vector<std::string> log_;
};

} // namespace mm::battle
