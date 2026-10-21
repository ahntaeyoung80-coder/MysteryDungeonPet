#!/bin/bash
set -euo pipefail
cd /repo
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/event-variety-final/finish-xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
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
