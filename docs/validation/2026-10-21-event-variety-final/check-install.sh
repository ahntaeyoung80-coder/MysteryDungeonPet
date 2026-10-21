#!/bin/bash
set -euo pipefail
cd /repo
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/event-variety-final/install-xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
/usr/lib/wine/wine64 build/event-variety-final/ui_run.exe Z:/repo/build/event-variety-final/install/MysteryDungeonPet.exe \
    --duration 6 --movement-trace Z:/repo/build/event-variety-final/install.csv \
    --metrics Z:/repo/build/event-variety-final/install-metrics.csv
