#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
required=['native/src/main.cpp','native/src/platform/input.cpp','native/src/platform/android_virtual_input.hpp',
'native/portmaster/AirXonix.sh','app/src/main/java/com/airxonix/nativeport/AirXonixActivity.java',
'app/build.gradle','build.gradle','settings.gradle','CMakeLists.txt',
'scripts/prepare_sdl2.sh','native/tests/test_android_touch_boot_r373.cpp']
missing=[name for name in required if not (root/name).is_file()]
if missing: raise SystemExit('Missing: '+str(missing))
activity=(root/'app/src/main/java/com/airxonix/nativeport/AirXonixActivity.java').read_text()
assert 'nativeTouchEvent' in activity and 'nativeTouchReset' in activity
assert 'vibrateOnDeath' in activity
print('PASS: Android r373 + H700 sources, Android touch, asset manifest')
