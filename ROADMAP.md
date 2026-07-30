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
- [ ] Camera (follow, map-edge clamp)
- [x] Multi-map loading, warps between maps (tile-triggered, save carries the
      current map name)
- [x] Entity set v1: NPC + Trainer from map JSON (`entities` array — sprite,
      facing, blocking, dialogue, authored teams); engine parses common
      fields, game-specific fields ride in `extra`
- [ ] Entity set v2: Item, Building, Warp, Trigger; NPC wander/facing sprites
- [x] Bitmap font rendering (`mm::Font`, generated ASCII atlas)
- [ ] Dialogue box + input routing for overworld UI (font exists; box is
      battle-only so far)
- [ ] Audio via miniaudio or SDL: BGM + SFX, data-driven per map/event
- [x] Save/load v1 (versioned JSON, `game/saves/save.json`): position/facing,
      defeated trainers, playtime, party with carried HP + status (battles
      start from party state; wipe = auto-heal); F5 save, F9 load, autosave
      on quit; healer NPC via `"heals": true` entity flag
- [ ] Save v2: script flags/vars, bag, boxes, multi-map position, migrations

## Phase 2 — Overworld gameplay

- [x] Dialogue box v1: speaker + wrapped lines from entity data, Z advances,
      trainer intro/defeated variants, flows into battles
- [ ] Dialogue v2: choices + conditionals (flag/var checks), typewriter text
- [x] Encounters: per-map tile-triggered tables (species/level range/weight/
      moves, encounter rate), weighted roll into a wild battle
- [ ] Inventory/bag, party management UI, PC storage boxes, Pokédex flags
- [x] Battle transition + battle scene v1 (pure view over the protocol log):
      animated HP bars, move menu driven by `Request`, faint-switch menu,
      humanized message box, sprite slots via `SpriteLibrary`, flee (Esc)
- [x] Catch flow: wild-only CATCH menu entry, classic catch-value formula off
      species `catchRate` + HP fraction + status, caught mon joins the party
- [ ] Battle scene v2: attack/faint animations, exp bar
- [x] Trainer battles from entity data (talk to fight, defeat remembered for
      the session, `teamFromJson` shared with saves/network)
- [ ] Trainer polish: line-of-sight engagement, reward money, rematch rules

## Phase 3 — Battle engine to competitive depth (highest priority track)

Runs in parallel with 1–2; `battle/` has no engine dependencies.

- [x] Status conditions (brn/par/psn/tox/slp/frz): infliction, immunities,
      turn gates (|cant|), residuals, burn/para stat effects, fire thaw
- [x] Stat boosts (-6..+6) via data-driven status moves (foe + self targets)
- [x] Secondary effect riders on damaging moves (chance + status)
- [x] Struggle (auto when PP dry) with 1/4 recoil
- [x] Regression suite (`battle_tests`, wired into CTest): stat formulas,
      type chart, determinism, status/boost/struggle golden scenarios
- [x] Volatiles: confusion, flinch, substitute (absorbs hits/status until it
      breaks, blocks opponent-targeted status moves while up)
- [ ] Remaining volatiles as content needs them (leech seed, taunt, encore, ...)
- [x] Abilities + items as data + hook points (Intimidate/Levitate/pinch-
      boost/contact-status; Leftovers/berries)
- [x] Weather: rain/sun (1.5x/0.5x water and fire damage), sandstorm/hail
      (1/16 max HP end-of-turn chip, rock/ground/steel and ice immune),
      5-turn duration set by field moves
- [ ] Terrain (electric/grassy/misty/psychic)
- [x] Protect (priority move, blocks the foe's hit, lapses unless re-chosen
      each turn), healing moves (% of max HP), recoil-by-fraction (% of
      damage dealt, distinct from Struggle's flat 1/4 max HP)
- [ ] Remaining move effect coverage (multi-hit, charge, switch moves,
      hazards)
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

## Phase 5 — Tools (started early: v1 shipped)

- [x] Dear ImGui integrated (SDL3 + SDL_Renderer backends), F1 editor mode
      with native-resolution UI over the paused game (docs/EDITOR.md)
- [x] Map editor v1: tile painting (ground/objects/collision), palette from
      the tileset image, JSON round-trip save preserving unknown fields
- [x] Entity editor: add/edit/delete/click-place NPCs + trainers, dialogue,
      trainer team JSON with validation
- [x] Species editor (stats/types/num) with live Dex reload; new-species flow
- [x] Team builder writing player.json (Dex-driven dropdowns)
- [x] Battle tester: headless run of any seed vs any map trainer, full log
- [x] Sprite studio v1: pixel editor saving PNGs into the hot-reload pipeline
- [x] Files panel: raw text editor + JSON validation for game/ data/scripts
- [ ] Map editor v2: warps, multi-map, resize, undo
- [ ] Sprite studio v2: animation frames/timing, origin, hitboxes, palettes
- [ ] Debug overlay during gameplay: entity inspector, battle state viewer,
      script console (needs Phase 4 Lua)
- [ ] Dex viewer (read-only browse with sprites)

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
