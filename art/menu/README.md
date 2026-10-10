# New retro-metallic AirXonix menu assets

Six newly generated, project-specific illustration-based sprite PNGs:
`MULTIPLAYER`, `HOST`, `JOIN`, `BACK`, `LIVES`, and `TIME`.
Their shape, brushed steel and gold embossed text evoke early-2000s 3D arcade game menus, without containing pixels copied from the original game's BMPPACK.

The generator `tools/generate_cleanroom_resources.py` packs the six sprites into `MPMENU` (256×512). A `MULTIPLAYER` variant also replaces the M105 main-menu label. Other M1/M2 entries are generated in a matching neutral-metal style. User-adjustable initial lives/time counters remain dynamic HUD text adjacent to their styled labels.

Original proprietary game graphics must not be added to this directory. Rebuild resources using `python3 tools/generate_cleanroom_resources.py assets`; it writes the distributable `AirXonix-cleanroom.zip` beside `assets/`.
