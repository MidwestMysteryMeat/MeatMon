# MeatMon Engine — Architecture

This document is the contract. Code that contradicts it is wrong, or this
document needs a deliberate edit first.

## 1. Identity and non-goals

MeatMon is a 2D engine for Pokémon-style RPGs: top-down tile overworld,
menu-driven battles, competitive-grade battle simulation, online play.

Hard non-goals — these never get built:

- No physics engine (movement is grid/tile logic).
- No 3D, no lighting/shadows, no post-processing.
- No particle system beyond timed sprite FX.
- No general-purpose ECS framework. Entities are a small closed set
  (Player, NPC, Trainer, Item, Building, Warp, Trigger).

Every feature request gets tested against: "does a Pokémon-like RPG with
competitive battles need this?" If no, it is out.

## 2. Module ownership

```
apps/game ──────────► engine ──────► SDL3 (window, renderer, input, audio later)
    │                    │
    │                    └─────────► stb (image decode), nlohmann/json
    │
    └────────────────► battle ─────► nlohmann/json only
apps/battle_cli ─────► battle
```

Rules:

- **`battle` never includes SDL or anything from `engine`.** It is a pure
  simulation library: deterministic, headless, unit-testable, and the exact
  same code runs on a dedicated server. This is the load-bearing wall of the
  whole design.
- **`engine` never includes `battle`.** Only apps compose the two. The battle
  *scene* (sprites, HP bars, animations) lives in the app/game layer and is a
  pure *view* over the battle protocol log.
- Game content lives in `game/` (data, maps, scripts, assets), never in C++.
  The engine executable plus a `game/` folder is the entire shippable product.

## 3. Main loop: fixed tick, interpolated render

- Simulation runs at a fixed **60 Hz** (`dt = 1/60`). An accumulator drains
  wall-clock time into zero or more `update()` calls per frame; rendering
  happens once per frame with `alpha = accumulator / dt` for interpolation.
- Frame time is clamped to 250 ms so debugger breaks don't spiral the
  accumulator.
- Simulation code never reads the wall clock and never touches unseeded
  randomness. All gameplay RNG flows through explicitly seeded generators.
  This is what makes replays, netplay, and battle verification possible.
- Renderer uses SDL's logical presentation (320×192 logical pixels,
  integer-scaled, nearest-neighbour) so pixel art stays crisp at any window
  size.

Interface: apps implement `mm::IGame` (`init`, `handleEvent`, `update(dt)`,
`render(renderer, alpha)`) and hand it to `mm::App::run()`.

## 4. Battle engine

### 4.1 Clean-room policy

The battle simulator is **inspired by the architecture of Pokémon Showdown**
(sim loop, request/choice protocol, `|`-delimited message log) but is written
from scratch in C++20 from publicly documented game mechanics (damage formula,
stat formula, type chart, PRNG constants — all long-published research).
No Showdown TypeScript/JS source is copied, ported line-by-line, or vendored.

**Naming rule for shipped content:** generic English words and descriptive
compounds anyone can use (Tackle, Headbutt, Growl, Leftovers, Intimidate,
nature adjectives) are fine. Distinctly Pokémon-coined proper nouns are not:
no species names, no invented item names (Oran/Sitrus → our Snack/Feast
Berry), no place/character names, and the word "Pokémon" itself never appears
in game data or the public API (`MonsterSet`, `BattleMonster`). Bundled
species (Emberling, Puddlit, Sprigling, Zapkin) are original placeholders,
and no Nintendo assets ship in this repo.

### 4.2 Data-driven Dex

All battle content loads at runtime from `game/data/`:

| File            | Contents                                              |
|-----------------|--------------------------------------------------------|
| `species.json`  | id → name, dex num (sprite key), types, base stats     |
| `moves.json`    | id → name, type, category, power, accuracy, priority, PP |
| `typechart.json`| attacking type → { defending type → multiplier }       |
| `natures.json`  | id → boosted/hindered stat                             |

`mm::battle::Dex::load(dir)` parses these into immutable tables. Hot-reload of
battle data means: throw away the Dex, load again, start a new battle. Battles
in flight are never mutated by a reload.

Abilities, items, and per-generation mechanics tables land in Phase 3 as more
JSON files plus (where behaviour is needed) Lua callbacks — never hardcoded
C++ switch statements per species.

### 4.3 Core objects

```
Dex          immutable rules/data tables
Battle       one battle: 2 Sides, RNG, phase machine, protocol log
Side         a player: team of BattleMonsters, active slot(s)
BattleMonster  in-battle state: computed stats, HP, move slots w/ PP
Request      "what may this side legally choose right now"
Choice       one side's decision (move X / switch to Y)
Prng         seeded 64-bit LCG (Gen-5 constants, Showdown-compatible next())
```

### 4.4 Request/choice flow (mirrors Showdown's protocol shape)

1. `Battle::start()` computes stats, switches in leads, emits
   `|player| |gametype| |teamsize| |start| |switch| |turn|1`.
2. Each side polls `request(side)` → `Move` (with legal move slots),
   `Switch` (after a faint), or `Wait`.
3. Sides submit `choose(side, Choice)`. Illegal choices return `false` and
   change nothing — the authoritative sim never trusts a client.
4. When all pending choices are in, `commitTurn()` resolves the turn:
   actions sort by priority ↓, speed ↓, seeded-RNG tiebreak; each move runs
   accuracy → immunity → crit → damage → faint checks, appending protocol
   messages (`|move| |-damage| |-supereffective| |faint| |win| ...`).
5. Faints put the battle in a `FaintSwitch` phase; replacements come in via
   `choose`, then the next `|turn|N` begins.

The **protocol log is the single source of truth for observers**. The battle
scene, spectators, and replays all consume the same `std::vector<std::string>`
log. Replay = the log itself. Network spectating = streaming the log.

### 4.5 Determinism and RNG

`Prng` is the Gen-5 64-bit LCG (`x' = x·0x5D588B656C078965 + 0x269EC3`, output
= top 32 bits), the same family Showdown uses. `next(n)` matches Showdown's
`floor(top32 · n / 2^32)`. Two modes are plumbed (`Showdown`, `Cartridge`);
they currently share the generator — the modes diverge later in *call order*
(damage roll/crit sequencing), which is Phase 3 work, tracked honestly in the
roadmap. Same seed + same choices ⇒ byte-identical log, on every platform.

### 4.6 Scaffold honesty (current stubs)

Implemented now: stat calc (EV/IV/nature), full 18-type chart, STAB, crits,
damage rolls, accuracy, priority/speed ordering with seeded ties, faint→switch
flow, win detection, protocol log, JSON state snapshot (`serialize()`);
status conditions (brn/par/psn/tox/slp/frz) with immunities, turn gates,
residuals, and burn/para stat effects; stat boosts (±6 stages) from
data-driven status moves; secondary-effect riders; Struggle with recoil.
A regression suite (`apps/battle_tests`, run via CTest) pins stat formulas,
the chart, determinism, and golden status/boost/struggle scenarios.
Stubbed: volatiles (confusion/flinch/etc.), abilities, items,
weather/terrain, multi-target formats (Doubles/Triples data model exists in
`Format` but mechanics enforce singles), full serialization round-trip.
See ROADMAP Phase 3.

## 5. Asset pipeline

### 5.1 Game-as-data

`game/` is the entire game. The engine binary is content-free. Shipping =
`meatmon.exe` + `game/` (that's what `tools/package.ps1` zips).

### 5.2 Sprite resolution (PokeAPI layout, first-class)

`mm::SpriteLibrary::resolve(query)` maps a species to an image path, searching
in order:

1. **Custom art** — `game/assets/custom/<slug>/front.png`, `back.png`,
   `front_shiny.png`, `back_shiny_female.gif`, … (`.gif` preferred over `.png`).
2. **PokeAPI drop-in** — `game/assets/pokemon/sprites/pokemon/` using the
   standard layout: `<num>.png`, `back/<num>.png`, `shiny/<num>.png`,
   `female/<num>.png` (nesting order: `back/` → `shiny/` → `female/`), and
   animated `versions/generation-v/black-white/animated/<num>.gif`.

Drop a cloned sprites repo into `game/assets/pokemon/` and every species with
a matching `num` gets art with zero code or data changes. That folder is
gitignored — sprite rips are copyrighted and stay on local disks only.

### 5.3 Decoding and hot-reload

- stb_image decodes PNG and GIF (all frames + per-frame delays → `SpriteFrames`).
- `AssetManager` caches by relative path and polls file mtimes (0.5 s). A
  changed file is reloaded **in place**: the `Texture*` handed out earlier
  stays valid, its GPU texture is swapped under it. Same pattern will cover
  data JSON and Lua scripts — holders keep stable handles, contents swap.
- A missing asset logs a clear path and the game keeps running; art iteration
  must never crash the runtime.

## 6. Overworld model

- `Tilemap`: JSON maps with `ground`, `objects`, `collision` layers (row-major
  int arrays into a single-row tileset image), `tile_size`, plus `warps` and
  `events` arrays (loaded, consumed from Phase 2 on).
- Movement is **grid-stepped**: an entity occupies a tile, animates to the
  next over N ticks, input is sampled only when aligned. Collision is a layer
  lookup, not physics.
- Entities are a closed set with plain-struct state; behaviour comes from Lua
  (Phase 4), not C++ subclasses.

## 7. Scripting model (Phase 4 contract, designed now)

- **Lua via sol2**, chosen for hot-reload maturity and tiny footprint.
- Scripts are *content*: they live in `game/scripts/` and define NPCs, items,
  events, cutscenes, and custom battle rules as data + callbacks
  (see `game/scripts/npcs/rival.lua` for the shape).
- Scripts receive a capability object (`ctx`) — dialogue, choices, warps,
  battle starts, flags/vars, inventory. They never touch SDL, files, or
  network directly. `require` is restricted to `game/scripts/`.
- Hot-reload: the watcher re-executes a changed script file; entity behaviour
  tables are re-bound by id. Persistent state lives in the save data, not in
  Lua globals, so a reload never corrupts a session.
- The battle sim exposes Lua hooks for custom formats/abilities in Phase 4+,
  keeping the core sim deterministic (hooks run inside the sim tick, with the
  sim's RNG).

## 8. Networking model (Phase 6, designed now)

- `Transport` interface (connect/accept, reliable-ordered + unreliable
  channels). Implementations: **Loopback** (single process, ships first),
  **ENet** (LAN/direct online), **Steam Networking** (lobbies/P2P, later).
- **Server-authoritative everything.** Battles: the server owns the `Battle`,
  clients receive `Request`s and protocol log lines, submit `Choice`s —
  exactly the local API, so single-player and netplay share one code path.
  Overworld: server validates movement/interactions; the only client
  prediction is the local player's walk animation.
- Because `battle` is renderer-free and deterministic, a headless dedicated
  server is just `battle_cli` grown up.

## 9. Saves

JSON with a versioned `"save_version"` field (migrations run oldest→newest).
v1 is live: position/facing, defeated trainers, playtime, and the party as
`MonsterSet` JSON with carried `hp`/`status` — battles start from party
state (`Battle::start` honours carried HP/status and skips fainted leads)
and write results back. Boxes, bag, dex flags, and script vars join in v2.
Binary packing is a Phase 7 optimization only if profiling demands it.

## 10. Conventions

- Namespaces: `mm` (engine), `mm::battle` (sim). Headers under
  `include/meatmon/…`, matching `.cpp` under `src/`.
- C++20, no exceptions across module boundaries except data-load failures
  (`Dex::load` throws `std::runtime_error` with the offending file/id).
- IDs everywhere are lowercase slugs (`"emberling"`, `"quickattack"`).
- All deps via FetchContent, pinned; no submodules, no system packages.
