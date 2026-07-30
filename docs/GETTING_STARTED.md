# Getting Started

## Build and run

Requirements: Visual Studio 2022 or 2026 with the "Desktop development with
C++" workload. Its bundled CMake works; no other installs — SDL3,
nlohmann/json, and stb are fetched and built automatically on first configure.

```powershell
cd F:\MeatMon
cmake --preset vs2026            # vs2022 on older installs
cmake --build --preset debug
```

Run from the repo root so the exe finds `game/`:

```powershell
build\apps\game\Debug\meatmon.exe
```

- Arrows / WASD — walk (grid-stepped, collision from the map's layer)
- **B** — run one battle turn; the Showdown-style protocol log prints to the
  console (`|move|`, `|-damage|`, `|faint|`, `|win|`…). When a battle ends,
  B starts a fresh one.
- Esc — quit

Headless battle (no window, full battle to completion + JSON snapshot):

```powershell
build\apps\battle_cli\Debug\battle_cli.exe            # default seed
build\apps\battle_cli\Debug\battle_cli.exe --seed 42  # different, reproducible
```

Smoke test (CI-friendly, exits 0/1): `meatmon.exe --selftest`

## Drop in your own sprites

**Custom art (always wins, commits fine):** create a folder under
`game/assets/custom/<slug>/` with `front.png` and optionally `back.png`,
`front_shiny.png`, `front.gif` (GIF beats PNG and animates). Done — no code,
no registry. If the game is running, saving a file hot-reloads it in ~half a
second; watch the console for `hot-reloaded …`.

**PokeAPI layout (local only, never committed):** copy a sprites repo into
`game/assets/pokemon/` so `game/assets/pokemon/sprites/pokemon/1.png` exists.
Every species whose `num` in `game/data/species.json` matches a file gets art
automatically, including `back/`, `shiny/`, `female/` variants and gen-5
animated GIFs. The `.gitignore` keeps this folder out of the repo.

Try it now: open `game/assets/custom/emberling/front.png` in any editor,
scribble on it, save — the corner sprite in the running game updates.

## Add a species

Append to `game/data/species.json`:

```json
"rockling": {
  "name": "Rockling",
  "num": 5,
  "types": ["rock"],
  "baseStats": { "hp": 50, "atk": 70, "def": 90, "spa": 35, "spd": 45, "spe": 30 }
}
```

Drop art at `game/assets/custom/rockling/front.png` (or a PokeAPI `5.png`).
Battle data reloads when a new battle starts; no rebuild.

## Write your first NPC script

Scripts live in `game/scripts/` and are plain Lua returning an entity table —
see the worked example at `game/scripts/npcs/rival.lua` (dialogue, a choice,
a battle with a custom team, a persistent flag). The `ctx` API contract is in
[SCRIPTING.md](SCRIPTING.md). The script *host* ships in Phase 4
(see ROADMAP.md); the format is authored-stable now so content written today
keeps working.

## Package a build

```powershell
tools\package.ps1        # -> dist/MeatMon.zip (exe + game/, sprite rips stripped)
```
