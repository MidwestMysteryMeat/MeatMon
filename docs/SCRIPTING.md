# Scripting Contract (Lua via sol2 — Phase 4 host, authored-stable now)

Scripts are game content: they live in `game/scripts/`, hot-reload on save,
and define NPCs, trainers, items, events, cutscenes, and custom battle rules.
This file is the API contract the Phase 4 script host implements; content
written against it today keeps working.

## Shape

Every script file returns a table. Entities declare data fields plus event
callbacks:

```lua
return {
  id = "oak_aide",                -- unique, lowercase slug
  name = "Aide",
  sprite = "custom/aide",         -- SpriteLibrary path
  map = "lab",                    -- which map owns this entity
  pos = { x = 4, y = 7 },
  facing = "down",
  wander = false,                 -- or { radius = 2 }

  on_interact = function(ctx) ... end,   -- player pressed A facing us
  on_step = function(ctx) ... end,       -- player stepped on our tile (Trigger)
  on_sight = function(ctx) ... end,      -- Trainer line-of-sight engaged
}
```

## The `ctx` capability object

Scripts touch the engine **only** through `ctx`. No SDL, no io/os, no network;
`require` resolves inside `game/scripts/` only.

| Call | Effect |
|---|---|
| `ctx:dialogue(speaker, text)` | Show a dialogue box, wait for confirm |
| `ctx:choice(prompt, {a, b, ...}) -> index` | Menu choice, 1-based |
| `ctx:flag(name) -> bool` / `ctx:set_flag(name)` | Persistent save flags |
| `ctx:var(name)` / `ctx:set_var(name, value)` | Persistent save variables |
| `ctx:give_item(id, count)` / `ctx:take_item(id, count) -> bool` | Bag |
| `ctx:party() -> table` | Read-only party summary |
| `ctx:heal_party()` | Center behaviour |
| `ctx:warp(map, x, y)` | Move the player |
| `ctx:face(dir)` / `ctx:move(dir, steps)` | Cutscene actor control (yields) |
| `ctx:battle(opts) -> { won, log }` | Run a battle (see below) |

`ctx:battle` options mirror the C++ `Battle` API: `format` (`"singles"` now;
doubles/triples with Phase 3), `opponent` display name, `team` as a list of
sets (`species`, `level`, `moves`, optional `nature`/`evs`/`ivs`), optional
`seed` for scripted deterministic fights. The callback receives the result
after the battle scene finishes; `log` is the protocol log for custom
post-battle logic.

## Rules that keep hot-reload safe

1. **No state in Lua globals.** Persistent state goes in flags/vars (saved);
   transient state in locals inside callbacks. A file reload re-executes the
   script and re-binds entities by `id` — globals would silently reset.
2. Callbacks are coroutines: `ctx:dialogue`/`ctx:move`/`ctx:battle` yield
   until done. Never busy-wait.
3. Determinism: scripts use `ctx`-provided RNG (`ctx:random(n)`, seeded by
   the sim) when the result affects gameplay. `math.random` is unseeded and
   reserved for cosmetic-only work.

## Custom battle rules (Phase 4+)

Formats, abilities, and move effects get Lua hook points registered from
`game/scripts/battle/`, executed inside the sim tick with the sim's RNG —
deterministic, replay-safe, netplay-safe. Contract lands with Phase 3's
effect system; tracked in ROADMAP.md.
