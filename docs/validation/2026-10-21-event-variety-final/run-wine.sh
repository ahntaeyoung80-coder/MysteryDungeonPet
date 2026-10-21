#!/bin/bash
set -euo pipefail
cd /repo
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/event-variety-final/xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
x86_64-w64-mingw32-g++ -municode -static build/event-variety-final/ui_run.cpp -o build/event-variety-final/ui_run.exe -luser32
mkdir -p build/event-variety-final/gui build/event-variety-final/fixtures
for name in pet_text_overlay_tests pet_npc_character_tests pet_dialogue_catalogue_tests pet_event_combat_tests pet_holiday_scene_tests; do
    /usr/lib/wine/wine64 "build/p0-windows14/$name.exe"
done
for entry in E01:15 E02:15 E03:18 E04:18 E05:15 E06:15 E07:18 E08:18 E09:15 share:15 trade:18 town:15 spring:15 bath:15 completion:18; do
    name=${entry%:*}; dungeon=${entry#*:}; duration=32
    case "$name" in town|spring) duration=105;; bath) duration=6;; completion) duration=42;; esac
    build/p0/pet_variety_scenario assets/original "$name" "$dungeon" "build/event-variety-final/fixtures/$name.mdes"
    /usr/lib/wine/wine64 build/event-variety-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
        --explore "Z:/repo/build/p0-windows14/assets/exploration/dungeon-$dungeon/exploration.mdpe" \
        --snapshot "Z:/repo/build/event-variety-final/fixtures/$name.mdes" --duration "$duration" \
        --movement-trace "Z:/repo/build/event-variety-final/gui/$name.csv" \
        --metrics "Z:/repo/build/event-variety-final/gui/$name-metrics.csv"
    build/p0/pet_variety_scenario assets/original inspect "$dungeon" "build/event-variety-final/fixtures/$name.mdes" >"build/event-variety-final/gui/$name-state.json"
    echo "GUI $name complete"
done
for profile in default meowth-munchlax munchlax-meowth female; do
    build/p0/pet_variety_scenario assets/original bath 15 "build/event-variety-final/fixtures/bath-$profile.mdes"
    /usr/lib/wine/wine64 build/event-variety-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
        --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-15/exploration.mdpe \
        --snapshot "Z:/repo/build/event-variety-final/fixtures/bath-$profile.mdes" --duration 5 \
        --character-profile "Z:/repo/build/event-variety-final/$profile.mdcp" \
        --movement-trace "Z:/repo/build/event-variety-final/gui/bath-$profile.csv"
done
build/p0/pet_variety_scenario build/event-variety-final/long-assets E01 15 build/event-variety-final/fixtures/long.mdes
/usr/lib/wine/wine64 build/event-variety-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
    --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-15/exploration.mdpe \
    --gameplay-assets Z:/repo/build/event-variety-final/long-assets/gameplay \
    --snapshot Z:/repo/build/event-variety-final/fixtures/long.mdes --duration 20 \
    --movement-trace Z:/repo/build/event-variety-final/gui/long.csv
build/p0/pet_variety_scenario assets/original pool 15 build/event-variety-final/fixtures/pool.mdes
/usr/lib/wine/wine64 build/event-variety-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
    --snapshot Z:/repo/build/event-variety-final/fixtures/pool.mdes --duration 42 \
    --movement-trace Z:/repo/build/event-variety-final/gui/pool.csv
/usr/lib/wine/wine64 build/event-variety-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
    --snapshot Z:/repo/build/event-variety-final/fixtures/pool.mdes --duration 5 \
    --movement-trace Z:/repo/build/event-variety-final/gui/pool-restart.csv
/usr/lib/wine/wine64 build/p0-windows14/pet_npc_character_tests.exe --fixtures Z:/repo/build/event-variety-final/npc-fixtures
for md in $(python3 -c 'import json; print(" ".join(str(c["mdIndex"]) for c in json.load(open("assets/original/gameplay/npcs/catalogue.json"))["characters"]))'); do
    for relation in friendly hostile; do
        name="$md-$relation"
        /usr/lib/wine/wine64 build/event-variety-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
            --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-15/exploration.mdpe \
            --snapshot "Z:/repo/build/event-variety-final/npc-fixtures/$name.mdes" --duration 4 \
            --movement-trace "Z:/repo/build/event-variety-final/gui/npc-$name.csv" \
            --metrics "Z:/repo/build/event-variety-final/gui/npc-$name-metrics.csv"
        echo "GUI NPC $name complete"
    done
done
for name in default restart; do
    /usr/lib/wine/wine64 build/event-variety-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
        --duration 18 --movement-trace "Z:/repo/build/event-variety-final/gui/$name.csv" \
        --metrics "Z:/repo/build/event-variety-final/gui/$name-metrics.csv"
done
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/events.log build/event-variety-final/wine-events.txt
