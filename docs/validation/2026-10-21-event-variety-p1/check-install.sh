#!/bin/bash
set -e
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/event-variety-p1/install-xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
/usr/lib/wine/wine64 build/event-variety-p1/ui_run.exe Z:/repo/build/event-variety-p1/install/MysteryDungeonPet.exe \
 --duration 4 --movement-trace Z:/repo/build/event-variety-p1/install.csv --metrics Z:/repo/build/event-variety-p1/install-metrics.csv
