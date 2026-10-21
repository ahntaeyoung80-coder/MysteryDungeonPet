#!/bin/bash
set -euo pipefail
cd /repo
mkdir -p build/notices-final/gui build/notices-final/fixtures
x86_64-w64-mingw32-g++ -municode -static docs/validation/2026-10-21-event-variety-final/ui_run.cpp -o build/notices-final/ui_run.exe -luser32
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/notices-final/xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
for name in pet_gameplay_notice_tests pet_feedback_regression_tests pet_text_overlay_tests; do
    /usr/lib/wine/wine64 "build/p0-windows14/$name.exe"
done
for entry in pickup:4 pickup-comment:6 pickup-apple:4 pickup-tm:4 ko:4 heal:4; do
    IFS=: read -r name duration <<< "$entry"
    build/p0/pet_variety_scenario assets/original "$name" 15 "build/notices-final/fixtures/$name.mdes"
    /usr/lib/wine/wine64 build/notices-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
        --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-15/exploration.mdpe \
        --snapshot "Z:/repo/build/notices-final/fixtures/$name.mdes" --duration "$duration" \
        --movement-trace "Z:/repo/build/notices-final/gui/$name.csv" --metrics "Z:/repo/build/notices-final/gui/$name-metrics.csv"
    build/p0/pet_variety_scenario assets/original inspect 15 "build/notices-final/fixtures/$name.mdes" >"build/notices-final/gui/$name-state.json"
    echo "PASS GUI $name"
done
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/events.log build/notices-final/wine-events.txt
