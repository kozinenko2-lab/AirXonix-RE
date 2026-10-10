## r388 — optional native bonus expansion

See [Russian implementation notes](R388-BONUSES-RU.md): a single **Новые бонусы (Да/Нет)** setting, five low-poly bonus types, the original `?` randomly awarding them when enabled, and new models/textures in the clean-room ZIP. Default **OFF** preserves original gameplay. AXMP v8 is required for two-player sessions.

# AirXonix RE — PortMaster / Anbernic H700 (r387)

A reverse-engineered native game core for the Anbernic RG40XX H class of Linux handhelds, using **ARMHF**, **SDL2** and **OpenGL ES2**. This branch contains the complete public source snapshot, not a standalone executable release.

## Highlights

- Original-style single-player gameplay, 3D presentation and input support.
- Two-player cooperative **local Wi-Fi** mode: shared lives, timer and score, blue/red players and trails, replicated world and damage events.
- **Multiplayer → Host / Join** inside the main game menu, using UDP discovery (27841) and TCP gameplay transport (27840 by default); no manual IP or second launcher.
- One PortMaster launcher: [`portmaster/AirXonix.sh`](portmaster/AirXonix.sh).
- Clean-room generated replacement assets through [`tools/generate_cleanroom_resources.py`](tools/generate_cleanroom_resources.py), including the new menu graphics.
- Extensive source-based regression tests in [`tests/`](tests/).

## Build

An ARMHF cross toolchain, SDL2 headers/libraries and GLES2-capable target userspace are needed for the H700 release. See [`scripts/build_h700_armhf.sh`](scripts/build_h700_armhf.sh) for the expected SDK variables, as well as [`README_MULTIPLAYER_RU.md`](README_MULTIPLAYER_RU.md) / [`README_MULTIPLAYER_EN.md`](README_MULTIPLAYER_EN.md) for installation and network conditions.

For a host Linux source compilation and targeted regression checks (not a substitute for H700 testing):

```bash
sudo apt-get install cmake ninja-build g++ libsdl2-dev libgles2-mesa-dev libpng-dev
cmake -S . -B build -G Ninja -DAIRXONIX_RESOURCE_FREE=ON -DAIRXONIX_BUILD_TESTS=ON
cmake --build build --target airxonix test_discovery_r386 test_multiplayer_menu_r386 -j2
ctest --test-dir build -R '^(discovery_r386|multiplayer_menu_r386)$' --output-on-failure
```

Generate clean-room assets separately with Python dependencies NumPy and Pillow. The generated `AirXonix-cleanroom.zip` is deliberately excluded from Git; place it next to the ELF in the game's PortMaster directory when assembling a distribution.

## Target installation

Use identical builds on both devices. One console selects **Host**, the other **Join**, on the same Wi-Fi LAN without client isolation. Both devices need firmware exposing the 32-bit ARMHF loader and functional GLES2. Hold **B + Select** to leave the session; log files are stored in the game folder (`AirXonix/logs/AirXonix.log`).

## Verification status

Local compilation and selected automated LAN/UI tests were previously performed for r386. **Full playthrough and real two-console H700 Wi-Fi tests have not been verified.** See the r386 release notes for details.

See the [Android branch](https://github.com/kozinenko2-lab/AirXonix-RE/tree/android) for an independent earlier Android r373 source snapshot. Do not merge the platform branches blindly.

## Authors, original-style art and enhancement plan

See [Credits](CREDITS.md), [Авторы и благодарности](CREDITS_RU.md), the new authored [metallic menu sprites](art/menu/), and [OpenGL ES2 graphics options proposal](docs/GRAPHICS_OPTIONS_RU.md). The original game remains credited to its authors and AxySoft; project textures do not include original commercial artwork. Optional gameplay shaders have not been implemented yet.

## r389 — графика и бонус Магнит

См. [описание r389](R389-CHANGES-RU.md) и [режимы OpenGL ES2](docs/GRAPHICS_OPTIONS_RU.md). В PortMaster: Настройки → Графика (ORIGINAL/SHARP/ENHANCED/TOON) и Новые бонусы (ДА/НЕТ).

## Primary H700 architecture: ARM64 (r390)

Native AArch64 is preferred; see [build guide](R390-ARM64.md). ARMHF is optional.
