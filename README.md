# AirXonix RE — Android ARM64 (r373)

Android Studio / Gradle source project for AirXonix native reimplementation. This branch is the **r373 public-source snapshot** and includes Android touch controls, SDL2 integration, GLES2 renderer, assets generator, and tests. This is **not** the same revision as PortMaster r386; the latter's LAN multiplayer menu has not been integrated into this branch.

## Prerequisites

- JDK 17, Android Gradle Plugin compatible with Gradle 8.7
- Android SDK Platform 35, Android NDK **27.2.12479018**, CMake 3.22.1
- Python 3, NumPy and Pillow for replacement textures
- Network access to clone **SDL2 release-2.30.12** or a previously prepared checkout in `third_party/SDL`

## Build without copyrighted game resources

```bash
python3 -m pip install numpy pillow
python3 native/tools/generate_cleanroom_resources.py generated/cleanroom
mkdir -p app/src/main/assets
cp generated/AirXonix-cleanroom.zip app/src/main/assets/
bash scripts/prepare_sdl2.sh
gradle --no-daemon --console=plain :app:assembleDebug
```

Install `app/build/outputs/apk/debug/app-debug.apk` on an Android ARM64 device (Android 8.0+). The project does not include private signing credentials; the debug APK is for development/testing.

The branch intentionally **omits** original commercial EXE/MUSIC/BMPPACK files, proprietary textures, private signing keys, prebuilt APKs, and generated archives. A separately provided personal game-data package must never be committed to this public repository.

### Input

- On-screen D-Pad/virtual stick and A/B buttons, multi-touch, Android software keyboard for high-score name input.
- SDL2 Bluetooth/USB gamepad support.
- Touch input implementation in [`native/src/platform/`](native/src/platform/) and Android bridge in [`app/src/main/java/`](app/src/main/java/).

### Continuous integration

The [Android GitHub Actions workflow](.github/workflows/android-apk.yml) builds a **clean-room debug APK** from this source tree. It does not require personal original assets or app-signing keys.

### Validation

This snapshot is the public r373 Android source tree. Compilation and runtime behavior on an actual handset must be checked with Android Studio or CI; no successful workflow run is claimed yet. Earlier internal build notes might reference original personal assets; those files are not in the public branch.

For the newer PortMaster coop implementation see [`portmaster`](https://github.com/kozinenko2-lab/AirXonix-RE/tree/portmaster).
