# AirXonix r386 — in-game Wi-Fi multiplayer

On two H700/Linux PortMaster handhelds connected to the same Wi-Fi network, launch the same `AirXonix.sh` and select **Multiplayer → Host** on one device and **Multiplayer → Join** on the other. Join auto-discovers compatible UDP beacons on port 27841 and connects via TCP port 27840 by default; no IP address entry is needed. During search, B cancels. In-game, hold B + Select for 1.2 seconds to return to the main menu.

New custom art is embedded in `AirXonix-cleanroom.zip` as `textures/M105.tga` (sixth main-menu strip) and `textures/MPMENU.tga` (title and HOST/JOIN/BACK graphics). The single launcher and all runtime logs live inside the game's PortMaster folder. Remove the obsolete secondary multiplayer launcher from old installations.

Both consoles must run matching r386 packages. Disable AP/client isolation and permit UDP 27841 and TCP 27840, or the configured `AIRXONIX_MP_PORT` TCP port. The runtime is ARMHF and requires the firmware's `/lib/ld-linux-armhf.so.3` interpreter and GLES2 driver. Two-device Wi-Fi/GLES testing remains necessary; loopback socket and game-core tests have passed.
