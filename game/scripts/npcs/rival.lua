-- First NPC script. This is the authoring shape the Phase 4 script host
-- executes (Lua via sol2, hot-reloaded on save). The `ctx` capability object
-- is the only way scripts touch the engine: dialogue, choices, flags, warps,
-- battles. Scripts never see SDL, files, or the network.
-- Full API contract: docs/SCRIPTING.md

return {
  id = "rival",
  name = "Rival",
  sprite = "custom/rival",          -- resolved by SpriteLibrary
  map = "demo",
  pos = { x = 10, y = 4 },
  facing = "down",

  on_interact = function(ctx)
    if ctx:flag("beat_rival_1") then
      ctx:dialogue("Rival", "Fine, you won. Once.")
      return
    end

    ctx:dialogue("Rival", "You again! My Puddlit is unbeatable.")
    local pick = ctx:choice("Battle now?", { "Bring it", "Not yet" })
    if pick == 1 then
      local result = ctx:battle({
        format = "singles",
        opponent = "rival",
        team = {
          { species = "puddlit", level = 5, moves = { "watergun", "tackle" } },
        },
      })
      if result.won then
        ctx:set_flag("beat_rival_1")
        ctx:dialogue("Rival", "No fair. Rematch later.")
      else
        ctx:dialogue("Rival", "Told you.")
      end
    else
      ctx:dialogue("Rival", "Thought so.")
    end
  end,
}
