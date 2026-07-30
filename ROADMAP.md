# MeatMon Engine — Roadmap

Scope test for every item: *required for a Pokémon-style 2D RPG with accurate
competitive battles?* If no, it doesn't get a phase.

## Phase 0 — Scaffold ✅ (this commit)

- [x] Repo layout, CMake + FetchContent (SDL3, nlohmann/json, stb), presets
- [x] ARCHITECTURE.md, ROADMAP.md, GETTING_STARTED.md, SCRIPTING.md
- [x] `mm::App`: window, SDL renderer, fixed 60 Hz tick + interpolated render
- [x] Texture/GIF loading (stb), `AssetManager` with mtime hot-reload
- [x] JSON `Tilemap` (ground/objects/collision) + demo map
- [x] `SpriteLibrary` resolving custom + PokeAPI sprite layouts
- [x] `meatmon_battle` v0: Dex (species/moves/typechart/natures), stats,
      damage calc, crits, STAB, accuracy, priority/speed order, seeded
      Showdown-style PRNG, request/choice flow, faint→switch, protocol log
- [x] `battle_cli` headless runner + `--selftest` smoke test in the game exe

## Phase 1 — Core 2D solidified

- [ ] Grid movement polish: facing, run/walk speeds, 4-dir character sheets
- [ ] Camera (follow, map-edge clamp), multi-map loading, warps between maps
- [ ] Entity set: NPC, Trainer, Item, Building, Warp, Trigger from map JSON
- [x] Bitmap font rendering (`mm::Font`, generated ASCII atlas)
- [ ] Dialogue box + input routing for overworld UI (font exists; box is
      battle-only so far)
- [ ] Audio via miniaudio or SDL: BGM + SFX, data-driven per map/event
- [ ] Save/load v1 (versioned JSON): position, flags, party

## Phase 2 — Overworld gameplay

- [ ] Dialogue system with choices + conditionals (flag/var checks)
- [ ] Encounters: grass/cave tables per map (species, levels, rates)
- [ ] Inventory/bag, party management UI, PC storage boxes, Pokédex flags
- [x] Battle transition + battle scene v1 (pure view over the protocol log):
      animated HP bars, move menu driven by `Request`, faint-switch menu,
      humanized message box, sprite slots via `SpriteLibrary`, flee (Esc)
- [ ] Battle scene v2: attack/faint animations, exp bar, catch flow
- [ ] Trainer battles from entity data (line-of-sight, reward money)

## Phase 3 — Battle engine to competitive depth (highest priority track)

Runs in parallel with 1–2; `battle/` has no engine dependencies.

- [x] Status conditions (brn/par/psn/tox/slp/frz): infliction, immunities,
      turn gates (|cant|), residuals, burn/para stat effects, fire thaw
- [x] Stat boosts (-6..+6) via data-driven status moves (foe + self targets)
- [x] Secondary effect riders on damaging moves (chance + status)
- [x] Struggle (auto when PP dry) with 1/4 recoil
- [x] Regression suite (`battle_tests`, wired into CTest): stat formulas,
      type chart, determinism, status/boost/struggle golden scenarios
- [ ] Volatiles (confusion, flinch, substitute, etc.)
- [ ] Abilities + items as data + hook points; weather and terrain
- [ ] Full move effect coverage (multi-hit, charge, protect, switch moves,
      hazards, healing, recoil-by-fraction)
- [ ] Doubles/Triples: multi-slot sides, targeting rules
- [ ] Generation mechanics switch (1–9): per-gen formula/typechart tables;
      cartridge vs Showdown RNG call-order parity
- [ ] Team validation (formats, clauses) + team import/export text format
- [ ] Full state serialization round-trip; expand the golden-log suite to
      per-generation fixtures checked in CI

## Phase 4 — Scripting & hot-reload everywhere

- [ ] Embed Lua via sol2 (FetchContent); `ctx` capability API per SCRIPTING.md
- [ ] NPC/item/trainer/cutscene scripts loaded from `game/scripts/`
- [ ] File watcher: Lua + JSON + PNG/GIF hot-reload (F5 forces full reload)
- [ ] Battle hooks in Lua for custom moves/abilities/formats
- [ ] Script console in the debug overlay

## Phase 5 — Tools

- [ ] Dear ImGui debug overlay: entity inspector, battle state viewer, console
- [ ] Map editor (in-engine): tile painting, layers, collision, warps, entities
- [ ] Sprite/animation editor: frame list, timing, origin, hitbox
- [ ] Team builder + Pokédex viewer; battle tester (any matchup, any seed)

## Phase 6 — Networking

- [ ] `Transport` interface + Loopback implementation
- [ ] ENet transport; server-authoritative battle sessions (Request/Choice
      over the wire, protocol log streamed to spectators)
- [ ] Overworld sync (server-validated movement, interaction locks)
- [ ] Replays: seed + choice stream + log; deterministic re-verification
- [ ] Steamworks: lobbies, P2P transport, invites (behind the same interface)

## Phase 7 — Packaging & projects

- [ ] `tools/package` → zip of exe + `game/` (exists as v0 PowerShell; add
      Linux, strip debug info, embed version)
- [ ] Template project generator (`tools/new_game`): minimal `game/` skeleton
- [ ] Linux build (CI matrix: MSVC + clang), Steam depot layout

## Deliberately never

Physics, 3D, lighting, particles-as-a-system, generic ECS, in-engine asset
store, C# bindings.
