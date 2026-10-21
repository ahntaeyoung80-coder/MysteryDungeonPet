#!/bin/bash
set -euo pipefail
cd /repo
mkdir -p build/feedback-final
x86_64-w64-mingw32-g++ -municode -static docs/validation/2026-10-21-event-variety-final/ui_run.cpp -o build/feedback-final/ui_run.exe -luser32
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/feedback-final/patrol-xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
build/p0/pet_variety_scenario assets/original patrol 15 build/feedback-final/fixtures/patrol-15.mdes
/usr/lib/wine/wine64 build/feedback-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
 --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-15/exploration.mdpe \
 --snapshot Z:/repo/build/feedback-final/fixtures/patrol-15.mdes --duration 8 \
 --movement-trace Z:/repo/build/feedback-final/gui/patrol-15.csv --metrics Z:/repo/build/feedback-final/gui/patrol-15-metrics.csv
/usr/lib/wine/wine64 build/p0-windows14/pet_entry_social_scene_tests.exe --fixtures Z:/repo/assets/original/exploration/dungeon-0/exploration.mdpe Z:/repo/build/feedback-final/legacy-fixtures
/usr/lib/wine/wine64 build/feedback-final/ui_run.exe Z:/repo/build/p0-windows14/MysteryDungeonPet.exe \
 --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-0/exploration.mdpe --solo \
 --snapshot Z:/repo/build/feedback-final/legacy-fixtures/npc.mdes --duration 5 \
 --movement-trace Z:/repo/build/feedback-final/gui/legacy-npc.csv
