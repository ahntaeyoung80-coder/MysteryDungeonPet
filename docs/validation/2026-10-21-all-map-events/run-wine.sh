#!/bin/bash
set -euo pipefail
cd /repo
mkdir -p build/all-map-events/gui build/all-map-events/fixtures
x86_64-w64-mingw32-g++ -municode -static docs/validation/2026-10-21-event-variety-final/ui_run.cpp -o build/all-map-events/ui_run.exe -luser32
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/all-map-events/xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
for name in pet_all_map_event_tests pet_event_combat_tests pet_event_movement_tests; do
    /usr/lib/wine/wine64 "build/p0-windows14/$name.exe"
done
for entry in E03:0:12 E04:0:18 E08:0:15 E03:15:12 E04:15:18 E08:15:15 E05:18:14; do
    IFS=: read -r name dungeon duration <<< "$entry"
    tag="$name-$dungeon"
    build/p0/pet_variety_scenario assets/original "$name" "$dungeon" "build/all-map-events/fixtures/$tag.mdes"
    /usr/lib/wine/wine64 build/all-map-events/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
        --explore "Z:/repo/build/p0-windows14/assets/exploration/dungeon-$dungeon/exploration.mdpe" \
        --snapshot "Z:/repo/build/all-map-events/fixtures/$tag.mdes" --duration "$duration" \
        --movement-trace "Z:/repo/build/all-map-events/gui/$tag.csv" --metrics "Z:/repo/build/all-map-events/gui/$tag-metrics.csv"
    build/p0/pet_variety_scenario assets/original inspect "$dungeon" "build/all-map-events/fixtures/$tag.mdes" >"build/all-map-events/gui/$tag-state.json"
    echo "PASS GUI $tag"
done
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/events.log build/all-map-events/wine-events.txt
