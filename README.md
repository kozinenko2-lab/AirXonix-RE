# AirXonix RE

Native AirXonix reverse-engineering project and maintained ports.

> **Status:** Experimental reimplementation. Anbernic H700 and Android hardware behavior is not guaranteed by compilation alone. This project is not affiliated with the original game's developers.

## Branches

| Branch | Platform | Version |
| --- | --- | --- |
| [**portmaster**](https://github.com/kozinenko2-lab/AirXonix-RE/tree/portmaster) | Anbernic RG40XX H / Allwinner H700 Linux; ARMHF, SDL2, OpenGL ES2, PortMaster | **r386** |
| [**android**](https://github.com/kozinenko2-lab/AirXonix-RE/tree/android) | Android ARM64, SDL2, OpenGL ES2, touch and controller support | **r373** |

Both branches contain full source trees, platform build instructions, test suites, and clean-room resource generation. The two ports have independent development histories; PortMaster multiplayer r386 is **not** an Android multiplayer implementation.

## Getting started

- [PortMaster installation and build notes](https://github.com/kozinenko2-lab/AirXonix-RE/blob/portmaster/README.md)
- [Android build notes](https://github.com/kozinenko2-lab/AirXonix-RE/blob/android/README.md)
- [Contribution guide](CONTRIBUTING.md)
- [Security policy](SECURITY.md)

## Distribution and license status

The Git history excludes the original commercial game's binary, audio, textures and resource packages, private Android signing keys, downloaded SDKs, compiled APKs/ELFs, and generated clean-room ZIPs. Generated replacement assets can be recreated locally with the included scripts.

**A project-wide source-code license has not yet been chosen.** Publishing code on GitHub does not by itself grant permission to reuse, relicense, or redistribute it; review provenance and choose a license before accepting external contributions. Trademarks and original copyrighted material remain with their respective owners.
