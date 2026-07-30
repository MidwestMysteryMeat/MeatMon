# PokeAPI sprite drop-in folder

Clone or copy a PokeAPI-layout sprites repository here so the folder looks
like:

```
game/assets/pokemon/sprites/pokemon/1.png
game/assets/pokemon/sprites/pokemon/back/1.png
game/assets/pokemon/sprites/pokemon/shiny/1.png
game/assets/pokemon/sprites/pokemon/female/3.png
game/assets/pokemon/sprites/pokemon/versions/generation-v/black-white/animated/1.gif
```

Any species whose `num` in `game/data/species.json` matches a file here gets
its art automatically — no code or data changes, and edits hot-reload while
the game runs.

**Nothing in this folder may be committed.** Real sprite rips are copyrighted
game assets; the `.gitignore` blocks everything here except this README. Ship
original art via `game/assets/custom/` instead.
