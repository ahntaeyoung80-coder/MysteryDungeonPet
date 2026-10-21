#!/bin/bash
set -e
export WINEDEBUG=-all WINEPREFIX=/tmp/mdpet-p0-wine DISPLAY=:99
mkdir -p /tmp/mdpet-xdg
chmod 700 /tmp/mdpet-xdg
export XDG_RUNTIME_DIR=/tmp/mdpet-xdg
Xvfb :99 -screen 0 1024x768x24 >build/event-variety-p0/xvfb.txt 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
for dll in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
    cp "$(x86_64-w64-mingw32-g++ -print-file-name="$dll")" build/p0-windows14/
done
/usr/lib/wine/wine64 build/p0-windows14/pet_variety_state_tests.exe Z:/repo/build/p0-wine-tests
/usr/lib/wine/wine64 build/p0-windows14/pet_package_transition_tests.exe assets/original/exploration/dungeon-0/exploration.mdpe assets/original/exploration/dungeon-15/exploration.mdpe
x86_64-w64-mingw32-g++ -static build/event-variety-p0/ui_control.cpp -o build/event-variety-p0/ui_control.exe -luser32
/usr/lib/wine/wine64 reg add 'HKCU\Software\Wine\Fonts\Replacements' /v 'Malgun Gothic' /d 'NanumGothic' /f
/usr/lib/wine/wine64 build/p0-windows14/MysteryDungeonPet.exe --duration 18 --movement-trace Z:/repo/build/event-variety-p0/default.csv --metrics Z:/repo/build/event-variety-p0/default-metrics.csv &
app_pid=$!
/usr/lib/wine/wine64 build/event-variety-p0/ui_control.exe 14000
wait "$app_pid"
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/exploration.mdes build/event-variety-p0/default-before-restart.mdes
/usr/lib/wine/wine64 build/p0-windows14/MysteryDungeonPet.exe &
app_pid=$!
/usr/lib/wine/wine64 build/event-variety-p0/ui_control.exe 3000
wait "$app_pid"
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/exploration.mdes build/event-variety-p0/default-after-restart.mdes
/usr/lib/wine/wine64 build/p0-windows14/MysteryDungeonPet.exe --explore Z:/repo/build/p0-windows14/assets/exploration/dungeon-15/exploration.mdpe --snapshot Z:/repo/build/event-variety-p0/default-before-restart.mdes --duration 6 --movement-trace Z:/repo/build/event-variety-p0/explicit.csv &
app_pid=$!
/usr/lib/wine/wine64 build/event-variety-p0/ui_control.exe 3000
wait "$app_pid"
cp /tmp/mdpet-p0-wine/drive_c/users/root/AppData/Local/MysteryDungeonPet/events.log build/event-variety-p0/wine-events.txt
