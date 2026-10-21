#!/bin/bash
set -e
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/event-variety-p1/xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
x86_64-w64-mingw32-g++ -municode -static build/event-variety-p1/ui_run.cpp -o build/event-variety-p1/ui_run.exe -luser32
for name in pet_npc_character_tests pet_character_scene_tests pet_dialogue_portrait_tests; do
    /usr/lib/wine/wine64 "build/p0-windows14/$name.exe"
done
/usr/lib/wine/wine64 build/p0-windows14/pet_npc_character_tests.exe --fixtures Z:/repo/build/event-variety-p1/gui-fixtures
mkdir -p build/event-variety-p1/gui
for md in 16 83 102 126 191 193 237 240 293 341 342 369 383 384 425 434 437; do
    for relation in friendly hostile; do
        case_name="$md-$relation"
        /usr/lib/wine/wine64 build/event-variety-p1/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
            --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-15/exploration.mdpe \
            --snapshot "Z:/repo/build/event-variety-p1/gui-fixtures/$case_name.mdes" --duration 4 \
            --movement-trace "Z:/repo/build/event-variety-p1/gui/$case_name.csv" \
            --metrics "Z:/repo/build/event-variety-p1/gui/$case_name-metrics.csv"
        echo "PASS GUI $case_name"
    done
done
for floor in 0 1 2 3 4 5; do
    /usr/lib/wine/wine64 build/event-variety-p1/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
        --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-15/exploration.mdpe --solo \
        --snapshot "Z:/repo/build/event-variety-p1/gui-fixtures/floor-$floor.mdes" --duration 4 \
        --movement-trace "Z:/repo/build/event-variety-p1/gui/floor-$floor.csv" \
        --metrics "Z:/repo/build/event-variety-p1/gui/floor-$floor-metrics.csv"
    echo "PASS floor cache $floor"
done
/usr/lib/wine/wine64 build/event-variety-p1/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
    --duration 18 --movement-trace Z:/repo/build/event-variety-p1/gui/default.csv \
    --metrics Z:/repo/build/event-variety-p1/gui/default-metrics.csv
/usr/lib/wine/wine64 build/event-variety-p1/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
    --duration 4 --movement-trace Z:/repo/build/event-variety-p1/gui/restart.csv \
    --metrics Z:/repo/build/event-variety-p1/gui/restart-metrics.csv
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/events.log build/event-variety-p1/wine-events.txt
