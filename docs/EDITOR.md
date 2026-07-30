# In-Engine Editor

Press **F1** in the overworld to open the editor; F1 again to close. The
gameplay canvas pauses while the editor is open. Every tool writes to the
same `game/` files the runtime loads, so edits are live: close the editor
(or let hot-reload catch the file change) and the game reflects them.

The map is shown at 2x on the left; panels are toggled from the **Panels**
menu. The **File** menu has *Save map* and *Reload data* (re-loads the Dex
and the player team after JSON edits).

## Panels

**Tiles** — pick a layer (ground / objects / collision) and paint with the
left mouse button; right button erases. The palette comes straight from the
map's tileset image, so adding tiles to `overworld.png` grows the palette.
Collision mode shows a red overlay. *Save map* writes the currently loaded
map file (e.g. `game/maps/demo.json` or `meadow.json`), preserving `warps`
and `encounters` verbatim even though there's no panel to edit them yet
(hand-author those in the Files panel or a text editor — structured warp/
encounter editing is Map editor v2, see ROADMAP.md).

**Entities** — list, add (+ NPC / + Trainer), delete, and edit entities:
position (type numbers or check *Place with click*, then click the map),
id, sprite slug, type, display name, dialogue (one line per row), and for
trainers the team as JSON with validation. Saved with the map.

**Species** — structured editor over `game/data/species.json`: name, dex
number (sprite key), one or two types, base stat sliders. *Apply & save*
writes the file and reloads the Dex immediately. *New* creates a species
with sane defaults — give it art via the Sprite studio and it's battle-ready
with zero code. `catchRate` and `baseExpYield` aren't in this panel yet;
edit them via the Files panel (or a text editor) until it catches up.

**Team builder** — edits `game/data/player.json` with dropdowns populated
from the Dex: species, level, nature, ability, item, four moves. Saving
reloads the player team, so the next battle uses it.

**Battle tester** — run a headless battle (your current team vs any trainer
on the map) with any seed, and read the full Showdown-style protocol log.
Same seed = same battle, so this is the tool for reproducing reports.

**Sprite studio** — a small pixel editor. Open any `assets/custom/<slug>`
front/back sprite or create a new 16/48 px one, paint with LMB (RMB erases,
alpha-aware color picker), then *Save PNG*. The runtime hot-reloads the file
within half a second — you can literally repaint a monster mid-battle.

**Files** — raw text editor for every `.json`, `.lua`, and `.md` under
`game/data`, `game/scripts`, and `game/maps`, with a JSON validate button.
This is the escape hatch when a structured panel doesn't cover a field yet.

## Notes

- The editor is Dear ImGui over the SDL renderer; it draws at native window
  resolution while gameplay stays on the 320x192 integer-scaled canvas.
- Editor tooling ships in the dev build only; the packaging step can strip
  it later (ROADMAP Phase 7).
- The `--selftest` run opens and closes the editor once, so CI catches
  ImGui/backend regressions.
