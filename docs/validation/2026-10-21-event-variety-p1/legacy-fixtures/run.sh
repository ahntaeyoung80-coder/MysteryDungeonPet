#!/bin/bash
set -euo pipefail
cd /repo
export WINEPREFIX=/tmp/mdpet-p0-wine WINEDEBUG=-all DISPLAY=:99 XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/p1-resume/xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
cmake --build build/p0-windows14 --target pet_companion_tests --parallel 2 >build/p1-resume/build.txt 2>&1
wine=/usr/lib/wine/wine64
package=Z:/repo/assets/original/exploration/dungeon-0/exploration.mdpe
output=Z:/repo/build/p1-resume
mkdir -p build/p1-resume/{follow,attack,friendly,hostile,navigation,entry,party}
launch() {
    "$wine" build/event-variety-p1/ui_run.exe Z:/repo/MysteryDungeonPet.exe \
        --explore "$package" --gameplay-assets Z:/repo/assets/original/gameplay \
        --snapshot "$output/$1/$2.mdes" --duration "$3" \
        --movement-trace "$output/$1/$4.csv" --metrics "$output/$1/$4-metrics.csv"
    echo "PASS EXE $1/$2/$4"
}
for name in follow attack; do
    "$wine" build/p0-windows14/pet_companion_tests.exe --fixtures "$package" "$output/$name"
    launch "$name" "$name" 4 trace
done
for name in friendly hostile; do
    "$wine" build/p0-windows14/pet_companion_tests.exe --relationship-fixtures "$package" "$output/$name"
    launch "$name" "$name" 1 trace
    "$wine" build/p0-windows14/pet_companion_tests.exe --inspect "$package" "$output/$name/$name.mdes" >"build/p1-resume/$name/state.json"
    launch "$name" "$name" 1 restored-trace
    "$wine" build/p0-windows14/pet_companion_tests.exe --inspect "$package" "$output/$name/$name.mdes" >"build/p1-resume/$name/restored-state.json"
done
"$wine" build/p0-windows14/pet_navigation_recovery_tests.exe --fixtures "$package" "$output/navigation"
launch navigation cycle 4 trace
"$wine" build/p0-windows14/pet_entry_social_scene_tests.exe --fixtures "$package" "$output/entry"
launch entry npc 6 trace
"$wine" build/p0-windows14/pet_companion_tests.exe --party-fixtures "$package" "$output/party"
launch party two-sided 16 trace
"$wine" build/p0-windows14/pet_npc_character_tests.exe --validate-fixtures "$package" "$output"
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/events.log build/p1-resume/events.txt
