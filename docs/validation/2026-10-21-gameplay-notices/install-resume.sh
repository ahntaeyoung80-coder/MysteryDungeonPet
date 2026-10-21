#!/bin/bash
set -euo pipefail
cd /repo
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/notices-final/install-xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
build/p0/pet_variety_scenario assets/original pickup 15 build/notices-final/fixtures/resume.mdes
for entry in before:1 after:3; do
    IFS=: read -r tag duration <<< "$entry"
    /usr/lib/wine/wine64 build/notices-final/ui_run.exe Z:/repo/build/notices-final/install/MysteryDungeonPet.exe \
        --explore Z:/repo/build/notices-final/install/assets/exploration/dungeon-15/exploration.mdpe \
        --snapshot Z:/repo/build/notices-final/fixtures/resume.mdes --duration "$duration" \
        --movement-trace "Z:/repo/build/notices-final/gui/resume-$tag.csv"
done
/usr/lib/wine/wine64 build/notices-final/ui_run.exe Z:/repo/build/notices-final/install/MysteryDungeonPet.exe \
    --duration 6 --movement-trace Z:/repo/build/notices-final/install.csv --metrics Z:/repo/build/notices-final/install-metrics.csv
