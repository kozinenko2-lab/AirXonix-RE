#!/usr/bin/env python3
"""Structural checks for Android exclusive event path.

The device still needs testing; this asserts Android never executes H700 raw
joystick fallbacks nor SDL synthetic keys.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/'native/src/platform/input.cpp').read_text()
java=(root/'app/src/main/java/com/airxonix/nativeport/AirXonixActivity.java').read_text()
main=(root/'native/src/main.cpp').read_text()
start=cpp.index('void InputSystem::poll(InputState& s) {')
android=cpp.index('#if defined(__ANDROID__)',start)
end=cpp.index('#else',android)
isolated=cpp[android:end]
assert 'SDL_JOYBUTTONDOWN' not in isolated
assert 'SDL_CONTROLLERBUTTONDOWN' not in isolated
assert 'SDL_GetKeyboardState' not in isolated
assert 'openFirstInputDevice' not in isolated
assert 'SDL_JoystickGetButton' not in isolated
assert 'const auto f=axTouch.poll(' in isolated
assert 's.action= (bits&AndroidVirtualInput::ENTER)!=0' in isolated
assert 's.back  = (bits&AndroidVirtualInput::ESCAPE)!=0' in isolated
assert 's={};' in isolated and 'SDL_QUIT' in isolated
assert 'SDL_GameControllerEventState(SDL_IGNORE);' in main
assert 'SDL_JoystickEventState(SDL_IGNORE);' in cpp
assert 'padOverlay.bringToFront()' in java
assert 'setClickable(true)' in java
assert 'nativeTouchEvent(bit,true)' in java
assert 'nativeTouchEvent(bit,false)' in java
print('r373 Android input isolated from H700 joystick: PASS (10 checks)')
