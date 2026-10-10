# AirXonix RE

Reverse-engineered native AirXonix implementation and platform ports maintained as separate branches.

> **Project status:** Experimental community reimplementation; hardware-dependent features must be tested on target devices. This repository is not affiliated with the original developers.

## Choose a branch

| Branch | Target | Snapshot | Key features |
| --- | --- | --- | --- |
| [`portmaster`](../../tree/portmaster) | Anbernic RG40XX H, H700 Linux / PortMaster, ARMHF, SDL2 and OpenGL ES2 | r386 | Cleanroom resource generator, native renderer, local Wi-Fi two-player coop, in-game Host/Join menu and LAN discovery |
| [`android`](../../tree/android) | Android (ARM64), SDL2 / OpenGL ES | r373 | Android Studio / Gradle project, touchscreen and virtual controller support |

The implementations share an AirXonix native game core but **are not automatically identical**: PortMaster r386 and Android r373 are different development snapshots. Changes should be ported between branches deliberately, built, and regression-tested.

## Build and source

- [PortMaster instructions](../../blob/portmaster/README_MULTIPLAYER_RU.md) (Russian) and [English](../../blob/portmaster/README_MULTIPLAYER_EN.md)
- [Android instructions](../../blob/android/README_ANDROID_RU.md) (Russian) and [English](../../blob/android/README_ANDROID_EN.md)
- Clean-room asset generation lives in the PortMaster [`tools/`](../../tree/portmaster/tools) directory. The generated archive is intentionally not versioned in Git.
- Tests and internal engineering notes are provided in each branch.

## Copyright, source provenance, and licenses

The repository contains code from our native reverse-engineering effort and public-source builds. Original commercial game files, proprietary audio/textures, private signing keys, SDKs/toolchains, compiled APKs, generated archives, and personal data are **not** committed.

No license grant for original proprietary AirXonix game assets is implied. A project-wide open-source license has not been selected yet; until it is explicitly selected, do not assume permission to redistribute or relicense code. The original game's trademarks belong to their respective owners.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Keep platform-specific changes on the appropriate branch, use small PRs, and attach build logs and applicable tests.
