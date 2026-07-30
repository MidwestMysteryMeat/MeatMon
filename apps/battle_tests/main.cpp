// Battle engine regression tests. No framework: CHECK counts failures,
// process exit code is the verdict (wired into CTest).
//
// Battles are deterministic per seed, so the status/struggle scenarios below
// are stable golden tests: if one starts failing, the sim's RNG consumption
// order or mechanics changed and that is exactly what we want to catch.

#include <meatmon/battle/battle.hpp>
#include <meatmon/battle/calc.hpp>
#include <meatmon/battle/team.hpp>

#include <cstdio>
#include <filesystem>
#include <string>

using namespace mm::battle;

static int failures = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

namespace {

bool logContains(const Battle& b, const std::string& needle) {
    for (const auto& line : b.log()) {
        if (line.find(needle) != std::string::npos) return true;
    }
    return false;
}

// Play up to `turns` turns; each side always picks the given move slot
// (the sim substitutes Struggle when PP runs dry) and front-fills switches.
void playTurns(Battle& b, int turns, int m1, int m2) {
    for (int t = 0; t < turns && !b.ended(); ++t) {
        for (int s = 0; s < 2; ++s) {
            Request r = b.request(s);
            if (r.kind == Request::Kind::Move) {
                b.choose(s, {ChoiceKind::Move, s == 0 ? m1 : m2});
            } else if (r.kind == Request::Kind::Switch && !r.switches.empty()) {
                b.choose(s, {ChoiceKind::Switch, r.switches.front()});
            }
        }
        b.commitTurn();
    }
}

Battle makeBattle(const Dex& dex, uint64_t seed,
                  std::vector<MonsterSet> p1, std::vector<MonsterSet> p2) {
    Battle b(dex, Format{}, seed);
    b.setPlayer(0, "P1", std::move(p1));
    b.setPlayer(1, "P2", std::move(p2));
    b.start();
    return b;
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path dataDir = "game/data";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--data" && i + 1 < argc) dataDir = argv[++i];
    }
    Dex dex = Dex::load(dataDir);

    // --- stat formulas (values verifiable by hand) ---------------------------
    CHECK(calc::hpStat(39, 31, 0, 50) == 114);        // Emberling L50 HP
    CHECK(calc::otherStat(65, 31, 0, 50, 100) == 85); // neutral speed
    CHECK(calc::otherStat(65, 31, 0, 50, 110) == 93); // +nature
    CHECK(calc::otherStat(65, 31, 0, 50, 90) == 76);  // -nature
    CHECK(calc::applyStage(100, 2) == 200);
    CHECK(calc::applyStage(100, -1) == 66);
    CHECK(calc::applyStage(100, 6) == 400);
    CHECK(calc::applyStage(100, -6) == 25);

    // --- type chart -----------------------------------------------------------
    CHECK(dex.effectiveness("water", {"fire"}) == 2.0);
    CHECK(dex.effectiveness("fire", {"water"}) == 0.5);
    CHECK(dex.effectiveness("fire", {"grass", "poison"}) == 2.0);
    CHECK(dex.effectiveness("electric", {"ground"}) == 0.0);
    CHECK(dex.effectiveness("normal", {"ghost"}) == 0.0);
    CHECK(dex.effectiveness("ice", {"grass", "flying"}) == 4.0);
    CHECK(dex.effectiveness("???", {"water"}) == 1.0);   // Struggle is typeless

    // --- determinism: same seed = byte-identical log ---------------------------
    auto runLog = [&](uint64_t seed) {
        Battle b = makeBattle(dex, seed,
            {{.species = "emberling", .moves = {"ember", "tackle"}}},
            {{.species = "puddlit", .moves = {"watergun", "tackle"}}});
        playTurns(b, 50, 0, 0);
        std::string all;
        for (const auto& l : b.log()) { all += l; all += '\n'; }
        return all;
    };
    CHECK(runLog(777) == runLog(777));
    CHECK(runLog(777) != runLog(778));

    // --- growl unboosts the foe ------------------------------------------------
    {
        Battle b = makeBattle(dex, 42,
            {{.species = "emberling", .moves = {"growl"}}},
            {{.species = "puddlit", .moves = {"growl"}}});
        playTurns(b, 1, 0, 0);
        CHECK(logContains(b, "|-unboost|p2a: Puddlit|atk|1"));
        CHECK(logContains(b, "|-unboost|p1a: Emberling|atk|1"));
    }

    // --- boosts clamp at -6 and report |-fail| ----------------------------------
    {
        Battle b = makeBattle(dex, 42,
            {{.species = "emberling", .moves = {"growl"}}},
            {{.species = "puddlit", .moves = {"growl"}}});
        playTurns(b, 8, 0, 0);   // 8 growls > 6 stages
        CHECK(logContains(b, "|-fail|p2a: Puddlit"));
    }

    // --- swords dance boosts self ------------------------------------------------
    {
        Battle b = makeBattle(dex, 7,
            {{.species = "emberling", .moves = {"swordsdance"}}},
            {{.species = "puddlit", .moves = {"growl"}}});
        playTurns(b, 1, 0, 0);
        CHECK(logContains(b, "|-boost|p1a: Emberling|atk|2"));
    }

    // --- thunder wave paralyzes (deterministic for this seed) --------------------
    {
        Battle b = makeBattle(dex, 1234,
            {{.species = "emberling", .moves = {"growl"}}},
            {{.species = "zapkin", .moves = {"thunderwave"}}});
        playTurns(b, 4, 0, 0);
        CHECK(logContains(b, "|-status|p1a: Emberling|par"));
        CHECK(logContains(b, "|-fail|p1a: Emberling"));   // re-statusing fails
    }

    // --- electric types are immune to paralysis ----------------------------------
    {
        Battle b = makeBattle(dex, 5,
            {{.species = "zapkin", .moves = {"growl"}}},
            {{.species = "zapkin", .moves = {"thunderwave"}}});
        playTurns(b, 4, 0, 0);
        CHECK(logContains(b, "|-immune|p1a: Zapkin"));
        CHECK(!logContains(b, "|-status|p1a: Zapkin|par"));
    }

    // --- toxic: status + escalating residual --------------------------------------
    {
        Battle b = makeBattle(dex, 99,
            {{.species = "emberling", .moves = {"growl"}}},
            {{.species = "puddlit", .moves = {"toxic"}}});
        playTurns(b, 4, 0, 0);
        CHECK(logContains(b, "|-status|p1a: Emberling|tox"));
        CHECK(logContains(b, "[from] tox"));
    }

    // --- poison types are immune to toxic -----------------------------------------
    {
        Battle b = makeBattle(dex, 99,
            {{.species = "sprigling", .moves = {"growl"}}},   // grass/poison
            {{.species = "puddlit", .moves = {"toxic"}}});
        playTurns(b, 4, 0, 0);
        CHECK(logContains(b, "|-immune|p1a: Sprigling"));
    }

    // --- sleep: |cant| then wake ----------------------------------------------------
    {
        Battle b = makeBattle(dex, 31337,
            {{.species = "sprigling", .moves = {"sleeppowder"}}},
            {{.species = "puddlit", .moves = {"growl"}}});
        playTurns(b, 6, 0, 0);
        CHECK(logContains(b, "|-status|p2a: Puddlit|slp"));
        CHECK(logContains(b, "|-curestatus|p2a: Puddlit|slp"));
    }

    // --- intimidate drops the foe's attack on switch-in ---------------------------
    {
        Battle b = makeBattle(dex, 11,
            {{.species = "emberling", .moves = {"growl"}}},
            {{.species = "puddlit", .ability = "intimidate", .moves = {"growl"}}});
        CHECK(logContains(b, "|-ability|p2a: Puddlit|Intimidate"));
        CHECK(logContains(b, "|-unboost|p1a: Emberling|atk|1"));
    }

    // --- levitate grants ground immunity ------------------------------------------
    {
        Battle b = makeBattle(dex, 12,
            {{.species = "puddlit", .moves = {"mudshot"}}},
            {{.species = "zapkin", .ability = "levitate", .moves = {"growl"}}});
        playTurns(b, 2, 0, 0);
        CHECK(logContains(b, "|-immune|p2a: Zapkin|[from] ability: Levitate"));
        CHECK(!logContains(b, "|-damage|p2a: Zapkin"));
    }

    // --- static can paralyze on contact (seed-pinned) ------------------------------
    {
        Battle b = makeBattle(dex, 13,
            {{.species = "emberling", .moves = {"tackle"}}},       // contact
            {{.species = "zapkin", .ability = "static", .moves = {"growl"}}});
        playTurns(b, 8, 0, 0);
        CHECK(logContains(b, "|-ability|p2a: Zapkin|Static"));
        CHECK(logContains(b, "|-status|p1a: Emberling|par"));
    }

    // --- leftovers heals each turn once damaged ------------------------------------
    {
        Battle b = makeBattle(dex, 14,
            {{.species = "puddlit", .item = "leftovers", .moves = {"growl"}}},
            {{.species = "emberling", .moves = {"tackle"}}});
        playTurns(b, 3, 0, 0);
        CHECK(logContains(b, "[from] item: Leftovers"));
    }

    // --- snack berry: eaten once when dropping below half --------------------------
    {
        Battle b = makeBattle(dex, 15,
            {{.species = "emberling", .item = "snackberry", .moves = {"growl"}}},
            {{.species = "puddlit", .moves = {"watergun"}}});
        playTurns(b, 10, 0, 0);
        CHECK(logContains(b, "|-enditem|p1a: Emberling|Snack Berry|[eat]"));
        CHECK(logContains(b, "[from] item: Snack Berry"));
    }

    // --- confusion: starts, activates, and either ends or self-hits ------------------
    {
        Battle b = makeBattle(dex, 16,
            {{.species = "emberling", .moves = {"growl"}}},
            {{.species = "puddlit", .moves = {"confuseray"}}});
        playTurns(b, 7, 0, 0);
        CHECK(logContains(b, "|-start|p1a: Emberling|confusion"));
        CHECK(logContains(b, "|-activate|p1a: Emberling|confusion") ||
              logContains(b, "|-end|p1a: Emberling|confusion"));
    }

    // --- headbutt flinch stops the slower side (seed-pinned) -------------------------
    {
        Battle b = makeBattle(dex, 17,
            {{.species = "zapkin", .moves = {"headbutt"}}},        // faster
            {{.species = "puddlit", .moves = {"growl"}}});
        playTurns(b, 8, 0, 0);
        CHECK(logContains(b, "|cant|p2a: Puddlit|flinch"));
    }

    // --- carried party state: HP override + fainted lead skipped -------------------
    {
        MonsterSet down{.species = "emberling", .moves = {"tackle"}};
        down.hp = 0;                                 // fainted from a prior fight
        MonsterSet hurt{.species = "sprigling", .moves = {"tackle"}};
        hurt.hp = 12;
        Battle b(dex, Format{}, 21);
        b.setPlayer(0, "A", {down, hurt});
        b.setPlayer(1, "B", {{.species = "puddlit", .moves = {"growl"}}});
        b.start();
        CHECK(b.side(0).active == 1);                // fainted lead skipped
        CHECK(b.side(0).monsters[1].hp == 12);
        CHECK(logContains(b, "|switch|p1a: Sprigling|Sprigling, L50|12/120"));
    }

    // --- carried status: burn ticks on turn 1 ---------------------------------------
    {
        MonsterSet burned{.species = "emberling", .moves = {"growl"}};
        burned.status = "brn";
        Battle b(dex, Format{}, 22);
        b.setPlayer(0, "A", {burned});
        b.setPlayer(1, "B", {{.species = "puddlit", .moves = {"growl"}}});
        b.start();
        playTurns(b, 1, 0, 0);
        CHECK(logContains(b, "[from] brn"));
    }

    // --- team JSON round-trip preserves carried state --------------------------------
    {
        MonsterSet s{.species = "zapkin", .level = 42, .nature = "timid",
                     .ability = "static", .item = "leftovers",
                     .moves = {"thundershock"}};
        s.hp = 7;
        s.status = "par";
        auto back = teamFromJson(teamToJson({s}));
        CHECK(back.size() == 1);
        CHECK(back[0].species == "zapkin" && back[0].level == 42);
        CHECK(back[0].hp == 7 && back[0].status == "par");
        CHECK(back[0].item == "leftovers" && back[0].nature == "timid");
    }

    // --- substitute: costs 1/4 max HP and blocks a status move the same turn --------
    {
        Battle b = makeBattle(dex, 50,
            {{.species = "emberling", .moves = {"substitute"}}},
            {{.species = "puddlit", .moves = {"growl"}}});
        playTurns(b, 1, 0, 0);   // Emberling (spe 65) acts before Puddlit (spe 43)
        CHECK(logContains(b, "|-start|p1a: Emberling|Substitute"));
        CHECK(b.side(0).monsters[0].hp == 114 - 114 / 4);
        CHECK(logContains(b, "|-fail|p2a: Puddlit"));           // growl blocked
        CHECK(!logContains(b, "|-unboost|p1a: Emberling|atk"));
    }

    // --- substitute absorbs hits until it breaks, then damage reaches real HP -------
    {
        Battle b = makeBattle(dex, 51,
            {{.species = "emberling", .moves = {"substitute", "growl"}}},
            {{.species = "puddlit", .moves = {"tackle", "tackle"}}});
        playTurns(b, 1, 0, 0);
        playTurns(b, 6, 1, 1);
        CHECK(logContains(b, "|-end|p1a: Emberling|Substitute"));
        CHECK(logContains(b, "|-activate|p1a: Emberling|Substitute|[damage]"));
    }

    // --- weather: rain sets the field, ticks for 5 turns, then clears ---------------
    {
        Battle b = makeBattle(dex, 60,
            {{.species = "puddlit", .moves = {"raindance", "growl"}}},
            {{.species = "emberling", .moves = {"growl"}}});
        playTurns(b, 1, 0, 0);
        CHECK(logContains(b, "|-weather|rain"));
        CHECK(b.weather() == "rain");
        playTurns(b, 4, 1, 0);   // 4 more turns: 5 total, weather should end
        CHECK(logContains(b, "|-weather|end|rain"));
        CHECK(b.weather().empty());
    }

    // --- sandstorm chips non-rock/ground/steel types each end of turn ---------------
    {
        Battle b = makeBattle(dex, 61,
            {{.species = "zapkin", .moves = {"sandstorm", "growl"}}},
            {{.species = "emberling", .moves = {"growl"}}});
        playTurns(b, 1, 0, 0);
        CHECK(logContains(b, "[from] sandstorm"));
    }

    // --- protect blocks the foe's move this turn only, then lapses -------------------
    {
        Battle b = makeBattle(dex, 70,
            {{.species = "emberling", .moves = {"protect", "growl"}}},
            {{.species = "puddlit", .moves = {"tackle", "tackle"}}});
        playTurns(b, 1, 0, 0);
        CHECK(logContains(b, "|-singleturn|p1a: Emberling|Protect"));
        CHECK(logContains(b, "|-activate|p1a: Emberling|Protect"));
        CHECK(b.side(0).monsters[0].hp == b.side(0).monsters[0].stats.hp);   // untouched
        playTurns(b, 1, 1, 1);   // protect not re-chosen: this tackle lands
        CHECK(b.side(0).monsters[0].hp < b.side(0).monsters[0].stats.hp);
    }

    // --- recover heals 50% of max HP, fails at full HP -------------------------------
    {
        MonsterSet hurt{.species = "puddlit", .moves = {"recover"}};
        hurt.hp = 10;
        Battle b(dex, Format{}, 71);
        b.setPlayer(0, "A", {hurt});
        b.setPlayer(1, "B", {{.species = "emberling", .moves = {"growl"}}});
        b.start();
        int maxHp = b.side(0).monsters[0].stats.hp;
        playTurns(b, 1, 0, 0);
        CHECK(logContains(b, "|-heal|p1a: Puddlit"));
        CHECK(b.side(0).monsters[0].hp > 10);
        CHECK(b.side(0).monsters[0].hp <= maxHp);
    }

    // --- double-edge: recoil off damage dealt, distinct from Struggle's flat 1/4 -----
    {
        Battle b = makeBattle(dex, 72,
            {{.species = "emberling", .moves = {"doubleedge"}}},
            {{.species = "puddlit", .moves = {"growl"}}});
        playTurns(b, 1, 0, 0);
        CHECK(logContains(b, "|-damage|p1a: Emberling") &&
              logContains(b, "[from] recoil"));
        CHECK(b.side(0).monsters[0].hp < b.side(0).monsters[0].stats.hp);
    }

    // --- struggle kicks in when PP runs dry, with recoil, and ends the battle -------
    {
        Battle b = makeBattle(dex, 314,
            {{.species = "emberling", .moves = {"growl"}}},
            {{.species = "puddlit", .moves = {"growl"}}});
        playTurns(b, 300, 0, 0);
        CHECK(logContains(b, "|Struggle|"));
        CHECK(logContains(b, "[from] recoil"));
        CHECK(b.ended());
    }

    if (failures == 0) {
        std::puts("battle_tests: ALL OK");
        return 0;
    }
    std::printf("battle_tests: %d failure(s)\n", failures);
    return 1;
}
