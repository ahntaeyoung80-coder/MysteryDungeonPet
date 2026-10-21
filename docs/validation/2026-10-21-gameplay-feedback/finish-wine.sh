#!/bin/bash
set -euo pipefail
cd /repo
mkdir -p build/feedback-final
x86_64-w64-mingw32-g++ -municode -static docs/validation/2026-10-21-event-variety-final/ui_run.cpp -o build/feedback-final/ui_run.exe -luser32
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/feedback-final/xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
mkdir -p build/feedback-final/gui build/feedback-final/fixtures
for name in pet_feedback_regression_tests pet_dialogue_catalogue_tests pet_text_overlay_tests pet_event_combat_tests; do
    /usr/lib/wine/wine64 "build/p0-windows14/$name.exe"
done
for entry in items:15:2 heal:15:4 intro:15:4 intro:18:4; do
    IFS=: read -r name dungeon duration <<< "$entry"
    tag="$name-$dungeon"
    build/p0/pet_variety_scenario assets/original "$name" "$dungeon" "build/feedback-final/fixtures/$tag.mdes"
    /usr/lib/wine/wine64 build/feedback-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
        --explore "Z:/repo/build/p0-windows14/assets/exploration/dungeon-$dungeon/exploration.mdpe" \
        --snapshot "Z:/repo/build/feedback-final/fixtures/$tag.mdes" --duration "$duration" \
        --movement-trace "Z:/repo/build/feedback-final/gui/$tag.csv" \
        --metrics "Z:/repo/build/feedback-final/gui/$tag-metrics.csv"
    build/p0/pet_variety_scenario assets/original inspect "$dungeon" "build/feedback-final/fixtures/$tag.mdes" >"build/feedback-final/gui/$tag-state.json"
    echo "PASS GUI $tag"
done
/usr/lib/wine/wine64 build/feedback-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
    --duration 30 --movement-trace Z:/repo/build/feedback-final/gui/default.csv \
    --metrics Z:/repo/build/feedback-final/gui/default-metrics.csv
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/events.log build/feedback-final/wine-events.txt
