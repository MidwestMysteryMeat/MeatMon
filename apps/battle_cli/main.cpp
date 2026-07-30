// Headless battle runner: proves the battle sim needs no engine/renderer and
// previews the dedicated-server shape. Prints the Showdown-style protocol log
// and a JSON state snapshot. Deterministic for a given --seed.

#include <meatmon/battle/battle.hpp>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>

using namespace mm::battle;

namespace {

// Deterministic demo "AI": cycle move slots by turn, skip empty PP.
int pickMove(int turn, const Request& req) {
    int n = static_cast<int>(req.moves.size());
    for (int k = 0; k < n; ++k) {
        int idx = (turn - 1 + k) % n;
        if (req.moves[idx].pp > 0) return idx;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path dataDir = "game/data";
    uint64_t seed = 0xC0FFEE;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) dataDir = argv[++i];
        else if (a == "--seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 0);
    }

    try {
        Dex dex = Dex::load(dataDir);

        Battle battle(dex, Format{}, seed);
        battle.setPlayer(0, "Red", {
            {.species = "emberling", .moves = {"ember", "quickattack", "tackle"}},
            {.species = "sprigling", .moves = {"vinewhip", "tackle"}},
        });
        battle.setPlayer(1, "Blue", {
            {.species = "puddlit", .moves = {"watergun", "tackle"}},
            {.species = "zapkin", .moves = {"thundershock", "quickattack"}},
        });
        battle.start();

        size_t cursor = 0;
        auto flush = [&] {
            for (; cursor < battle.log().size(); ++cursor) {
                std::puts(battle.log()[cursor].c_str());
            }
        };
        flush();

        int guard = 0;
        while (!battle.ended() && guard++ < 100) {
            for (int s = 0; s < 2; ++s) {
                Request req = battle.request(s);
                if (req.kind == Request::Kind::Move) {
                    battle.choose(s, {ChoiceKind::Move, pickMove(battle.turn(), req)});
                } else if (req.kind == Request::Kind::Switch && !req.switches.empty()) {
                    battle.choose(s, {ChoiceKind::Switch, req.switches.front()});
                }
            }
            battle.commitTurn();
            flush();
        }

        std::printf("\n--- serialized state (network/replay snapshot) ---\n%s\n",
                    battle.serialize().c_str());
        return battle.ended() ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "battle_cli: %s\n", e.what());
        std::fprintf(stderr, "hint: run from the repo root or pass --data <path-to-game/data>\n");
        return 1;
    }
}
