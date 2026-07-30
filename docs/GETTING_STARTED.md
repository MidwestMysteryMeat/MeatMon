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

- Arrows / WASD — walk (grid-stepped, collision from the map's layer; NPCs block)
- **Z / Enter** — talk to the NPC or trainer you're facing. Trainer dialogue
  flows straight into a battle with their authored team; beat them and their
  dialogue changes.
- **B** — shortcut: fight the map's trainer immediately
- In battle: arrows pick a move, Z/Enter confirms, Esc flees. The scene is a
  pure view over the same Showdown-style protocol log the headless CLI prints.
- **F1** — open the in-engine editor: map/tile painting, entity editor,
  species editor, team builder, battle tester, sprite pixel editor, and a
  raw file editor. See [EDITOR.md](EDITOR.md).
- **F5 / F9** — save / load (`game/saves/save.json`, versioned JSON). Quitting
  autosaves. Party HP, status, and EXP persist between battles — the
  villager heals your team (any entity with `"heals": true` does).
- Esc (overworld) — save and quit

Walking onto a warp tile loads the target map (see `game/maps/meadow.json`
for the shape); walking onto a tall-grass tile can trigger a wild encounter
from that map's `encounters` table, dropping you into a battle with a
CATCH option. Winning any battle — trainer or wild — levels up whichever
monster was on the field when it ended; a catch does not grant EXP.

## Author an NPC or trainer (no code)

Entities live in the map file (`game/maps/demo.json`, `"entities"` array).
An NPC is a sprite slug, a tile position, and dialogue lines; a trainer adds
a `team` (same JSON shape as `game/data/player.json` — species, level,
ability, item, moves, optional nature/EVs/IVs) and an optional
`defeatedDialogue`. Save the file while the game runs — the map hot-reloads,
entities included. The player's own team is `game/data/player.json`.

Headless battle (no window, full battle to completion + JSON snapshot):

```powershell
build\apps\battle_cli\Debug\battle_cli.exe            # default seed
build\apps\battle_cli\Debug\battle_cli.exe --seed 42  # different, reproducible
```

Smoke test (CI-friendly, exits 0/1): `meatmon.exe --selftest`

Headless screenshot (auto-exits, no interaction needed):

```powershell
build\apps\game\Debug\meatmon.exe --shot "30:out.png"
```

`--shot` is repeatable (`--shot "30:a.png" --shot "90:b.png"`); each captures
the frame at that tick to a PNG via `SDL_RenderReadPixels`. Combine with
`--selftest` to piggyback its scripted trainer battle and F1 editor toggle —
that's how `docs/screenshots/*.png` were generated.

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
  "baseStats": { "hp": 50, "atk": 70, "def": 90, "spa": 35, "spd": 45, "spe": 30 },
  "catchRate": 90,
  "baseExpYield": 65
}
```

`catchRate` (1-255, higher = easier) and `baseExpYield` (EXP awarded, scaled
by the defeated mon's level) are both optional — they default to 128 and 60
if omitted. Drop art at `game/assets/custom/rockling/front.png` (or a
PokeAPI `5.png`). Battle data reloads when a new battle starts; no rebuild.

## Add a move effect

Moves are data too — `game/data/moves.json` composes effects from optional
fields rather than code per move:

| Field | Effect |
|---|---|
| `status` / `volatile` (+ `target: "self"`) | Inflict a status/volatile on self or foe |
| `secondary: { chance, status, volatile }` | Chance-based rider on a damaging hit |
| `boosts: { atk: 1, ... }` | Stat stage changes (-6..+6) |
| `weather` | `"rain"`/`"sun"`/`"sandstorm"`/`"hail"`, 5 turns |
| `hazard` | `"spikes"` or `"stealthrock"`, targets the foe's side |
| `healPercent` | Heals this % of max HP |
| `recoilPercent` | User takes this % of damage dealt as recoil |
| `minHits` / `maxHits` | Multi-hit (equal = fixed count, otherwise weighted 2-5) |
| `charge: true` | Two-turn move: prepare, then auto-fires next turn |
| `protect: true` | Blocks the foe's hit this turn only (needs `priority: 4`-ish to go first) |
| `contact: true` | Triggers Static-style contact abilities |

See `battle/include/meatmon/battle/dex.hpp` (`Move` struct) for the
authoritative field list, and ARCHITECTURE.md §4.2 for how this composes
with abilities/items. A new *mechanic* (not just a new use of an existing
field) is a small `Battle::executeMove` change, not a data-only addition.

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
