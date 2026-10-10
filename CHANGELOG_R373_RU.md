# AirXonix Android r373 — isolated touchscreen controls

- Android now ignores **all** SDL virtual/hardware joystick, raw button and SDL keyboard game-command events (except SDL_TEXTINPUT exclusively for the name field).
- Removes phantom A/Enter triggered by raw joystick button zero on Android before the first touch.
- All Android controls originate only from the on-screen Java overlay through the existing r372 JNI event queue.
- Java overlay made clickable, brought to front; touch mask visible at the top-left for phone testing; adb logging in `AirXonixTouch` shows `UI` and `GAME` events.
- A = Enter, B = Escape, stick = arrow keys. H700 input code path and PortMaster build unaffected.
- **Diagnostic build**: physical Bluetooth/USB gamepads are temporarily ignored on Android, until touch operation is confirmed.
- Correctness still requires actual Android device testing: native unit tests cannot prove Java touch delivery on all phone models.
