# MeatMon Engine

A ruthlessly scoped 2D engine for Pokémon-style RPGs with competitive-grade
battles. C++20, SDL3, data-driven everything.

**What it is:** a fixed-tick 2D tilemap engine plus a clean-room,
Showdown-inspired battle simulator, built as two independent libraries under
one roof. **What it is not:** no physics engine, no 3D, no lighting, no
particle systems. Every line exists to ship a monster-catching RPG with
accurate, server-authoritative battles.

## Layout

```
battle/     meatmon_battle  — pure C++20 battle sim (no SDL, no rendering)
engine/     meatmon_engine  — window, 60 Hz tick loop, assets, tilemap, sprites
apps/
  game/        the game executable (engine + battle)
  battle_cli/  headless battle runner, prints Showdown-style protocol
game/       game-as-data: data/, maps/, scripts/, assets/  (ships with the exe)
docs/       GETTING_STARTED.md, SCRIPTING.md
tools/      package.ps1, gen_placeholders.ps1
```

## Build (Windows)

Requires Visual Studio 2022/2026 with the C++ workload (bundled CMake is fine).
All dependencies are fetched automatically — no vcpkg, no system installs.

```powershell
cmake --preset vs2026          # or vs2022
cmake --build --preset debug
```

Run from the repo root (the exe looks for the `game/` folder):

```powershell
build\apps\game\Debug\meatmon.exe                 # windowed demo
build\apps\battle_cli\Debug\battle_cli.exe        # headless battle, full protocol log
build\apps\game\Debug\meatmon.exe --selftest      # 4-second smoke test, exit code 0/1
```

Demo controls: arrows/WASD move, Z talks (trainer dialogue flows into a full
battle scene), B quick-battles, **F1 opens the in-engine editor** (map
painting, entities, species, team builder, battle tester, sprite pixel
editor — see docs/EDITOR.md), Esc quits.

## Documents

- [ARCHITECTURE.md](ARCHITECTURE.md) — ownership, tick loop, battle API, asset
  pipeline, scripting and networking model.
- [ROADMAP.md](ROADMAP.md) — phases from this scaffold to Steam packaging.
- [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) — drop in your own
  sprites, add a species, write your first NPC script.

## License

Private, proprietary. All rights reserved. No Pokémon assets or Pokémon
Showdown source are included or copied; see the clean-room policy in
ARCHITECTURE.md.
