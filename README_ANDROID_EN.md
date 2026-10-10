# AirXonix Android r373 — public source tree

This is the **source** for the ARM64 Android build: Gradle/Android application, native SDL2+GLES2 implementation, Java bridge, touch/virtual controls and tests. For full setup use [README.md](README.md).

## Build using generated replacement assets

Requirements: JDK 17, Gradle 8.7, Android SDK API 35, Android NDK 27.2.12479018, CMake 3.22.1, Python 3 with NumPy and Pillow, and SDL2 2.30.12 source.

```bash
python3 -m pip install numpy pillow
python3 native/tools/generate_cleanroom_resources.py generated/cleanroom
mkdir -p app/src/main/assets
cp generated/AirXonix-cleanroom.zip app/src/main/assets/
bash scripts/prepare_sdl2.sh
gradle --no-daemon :app:assembleDebug
```

The debug APK is written to `app/build/outputs/apk/debug/app-debug.apk`. The workflow in `.github/workflows/android-apk.yml` should run these same steps on GitHub Actions.

**Public-source note:** Commercial EXE/MUSIC/BMPPACK files, private signed builds, and Android signing keys are deliberately omitted. The public GitHub branch does not include any user's personal original-game package.

Touch input supports a virtual analog stick mapped to cardinal directions, A/B, multitouch, name-entry soft keyboard, and gamepads.

Build success and handset behavior must be confirmed by CI/device testing. For H700 Wi-Fi coop, see the separate [portmaster branch](https://github.com/kozinenko2-lab/AirXonix-RE/tree/portmaster).
