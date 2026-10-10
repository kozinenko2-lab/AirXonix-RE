#!/bin/bash
# AirXonix r389 — single PortMaster launcher (ARMHF / Allwinner H700).
# Single player and Wi-Fi Host/Join are selected from INSIDE the game.
set -u
PORT_NAME="AirXonix"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT_ROOT=""
PORT_DIR=""
for root in /mnt/sdcard/ports /mnt/mmc/ports /roms/ports /roms2/ports /storage/roms/ports /userdata/roms/ports "$SCRIPT_DIR"; do
    if [ -d "$root/$PORT_NAME" ]; then PORT_ROOT="$root"; PORT_DIR="$root/$PORT_NAME"; break; fi
done
if [ -z "$PORT_DIR" ]; then
    echo "ERROR: AirXonix game folder not found next to launcher or in PortMaster directories"
    exit 127
fi
mkdir -p "$PORT_DIR/logs" "$PORT_DIR/saves"
LOG="$PORT_DIR/logs/AirXonix.log"
: > "$LOG"
exec >>"$LOG" 2>&1
printf 'AirXonix r389 (single launcher) %s\n' "$(date 2>/dev/null || true)"
echo "PORT_DIR=$PORT_DIR architecture=$(uname -m 2>/dev/null || echo unknown)"
for ctrl in \
    "$PORT_ROOT/PortMaster/control.txt" \
    /mnt/mmc/MUOS/PortMaster/control.txt \
    /mnt/sdcard/MUOS/PortMaster/control.txt \
    /roms/ports/PortMaster/control.txt \
    /roms2/ports/PortMaster/control.txt \
    /storage/roms/ports/PortMaster/control.txt \
    /userdata/roms/ports/PortMaster/control.txt; do
    if [ -f "$ctrl" ]; then
        # shellcheck disable=SC1090
        source "$ctrl"
        if declare -F get_controls >/dev/null; then get_controls || true; fi
        break
    fi
done
if declare -F pm_platform_helper >/dev/null; then pm_platform_helper || true; fi
[ -n "${sdl_controllerconfig:-}" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export HOME="$PORT_DIR/saves" XDG_DATA_HOME="$PORT_DIR/saves"
export LD_LIBRARY_PATH="$PORT_DIR/libs-armhf:${LD_LIBRARY_PATH:-}"
export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0
export SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0
export AIRXONIX_VIBRATION_ENABLED="${AIRXONIX_VIBRATION_ENABLED:-1}"
export AIRXONIX_VIBRATION_DURATION_MS="${AIRXONIX_VIBRATION_DURATION_MS:-260}"
export AIRXONIX_VIBRATION_STRENGTH="${AIRXONIX_VIBRATION_STRENGTH:-82}"
# The discovery protocol sends a UDP beacon on 27841 and advertises this TCP port.
export AIRXONIX_MP_PORT="${AIRXONIX_MP_PORT:-27840}"
# Never accidentally import the legacy second-launcher host/join selection.
unset AIRXONIX_MP_ROLE AIRXONIX_MP_HOST
unset SDL_VIDEODRIVER SDL_AUDIODRIVER
if [ ! -e /lib/ld-linux-armhf.so.3 ]; then
    echo 'ERROR: This firmware lacks the 32-bit ARMHF loader (/lib/ld-linux-armhf.so.3).'
    echo 'Use firmware with ARMHF userspace compatibility.'
    exit 126
fi
BIN="$PORT_DIR/airxonix"
[ -x "$BIN" ] || { echo "ERROR: missing game binary $BIN"; exit 127; }
[ -r "$PORT_DIR/AirXonix-cleanroom.zip" ] || { echo 'ERROR: missing cleanroom resources'; exit 127; }
cd "$PORT_DIR" || exit 127
echo 'Opening single player / Multiplayer menu. Wi-Fi UDP 27841 and TCP 27840.'
"$BIN"
RC=$?
echo "AirXonix stopped: $RC"
exit "$RC"
