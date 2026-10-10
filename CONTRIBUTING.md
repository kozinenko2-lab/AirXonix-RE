# Contributing

1. File a GitHub issue for reproducible bugs and state the target branch (`portmaster` or `android`).
2. Start changes from the relevant branch. Keep pull requests narrow and document how to test them.
3. For PortMaster: include the console model, firmware, launcher log, SDL2/GLES2 version, and whether a second Wi-Fi console was present.
4. For Android: include Android version, ABI, device GPU, relevant `adb logcat` excerpts, and touch/joystick behavior.
5. Do not commit original game's copyrighted content, ROM dumps, proprietary translation packages, private keystores, passwords, binaries, prebuilt toolchains, or generated output.
6. Do not present source-level compilation as proof of behavior on physical devices. Mention test limitations explicitly.
7. Use conventional, descriptive commit subjects (e.g. `fix(net): retry LAN discovery after disconnect`).

Maintainers must decide a project license before accepting externally contributed code, to avoid ambiguity over redistribution rights.
