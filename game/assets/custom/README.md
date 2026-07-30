# Custom sprite folder

One folder per species/character slug. Files are checked before the PokeAPI
drop-in layout, so custom art always wins.

```
custom/<slug>/front.png            required
custom/<slug>/back.png             battle back view
custom/<slug>/front_shiny.png      optional variants
custom/<slug>/back_shiny.png
custom/<slug>/front_female.png
custom/<slug>/front.gif            .gif beats .png when both exist (animated)
```

Drop a folder in while the game is running: it is found on next lookup, and
saving over an existing file hot-reloads it in place.

The PNGs here are generated placeholders (see `tools/gen_placeholders.ps1`) —
original art, safe to commit.
