# Procedural native bonus meshes (AXBM v1)

Six low-poly meshes: Shield, Snowflake, LivesX2, ScoreX2, SafeTail, Magnet.
Their model and atlas data is authored/generated, not extracted from the game.
`tools/generate_bonus_models.py` is the source; it creates
`models/bonus_models.axbm` (six mesh entries) and `textures/BONUSX.tga`
(256x384, six UV strips). The game loads these from AirXonix-cleanroom.zip.
Original six bonus records and original `?` visual resource are unchanged.
Enable *New bonuses* in Settings to enable the extended pool and magnet.
