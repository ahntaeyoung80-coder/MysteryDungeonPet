# 이동·카메라 검증 재현

2026-10-15 `work`의 자동 검증 범위 완료. [명세](MOVEMENT_STABILIZATION_SPEC.md), [검증 요약](validation/2026-10-15-movement-summary.json), [단계별 기록](logs/2026-10-15-movement-stage4.md)을 함께 본다.

## 실행 결과

- Linux Release9/9, ASan/UBSan9/9, Python26/26 통과.
- [CI38666625639](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38666625639): Linux·Windows 성공. Windows Server2025/MSVC19.51 native10/10 통과.
- Windows 고정31시나리오의2,362합성frame·2,055화면 제출 통과. 직선·방향 전환을1/2/3배에서 확인하고, 네 변/네 모서리24시나리오의alpha255·좌표·동일 frame 불변을 확인했다.
- 실제 프로세스를 이동 도중 종료한 뒤 같은 snapshot으로 재실행했다. 남은 이동과 runId·inventory·once journal·day·HP 유지가 통과했다.
- 기존 생활·전투·층 전환·설치/루트 EXE 기본 실행·저장 복원도 통과했다. 원본 ROM·압축본은 그대로다.

이동·카메라의 런타임 결함은 이번 시나리오에서 재현되지 않았다. 변경은 검증 도구와 선택형 프레임 기록이다. 논리 이동은400ms이며 해당 CI의 실제 행동 시작 간격은400~413ms(중앙값412ms)였다. timer·OS 스케줄링·durable 저장 지연이 실측에 포함된다.

실제 사용자 Windows11 육안, 잠금·절전·DPI/모니터 사건, 장기 성능·전체 콘텐츠는 후속 범위다. 기존 무작위4초 관찰의 대화/전투 보류 기록은 그대로 보존하며, 고정 시나리오의 이동 증거를 별도로 제시한다.

## Linux 코어 검사

```sh
cmake -S . -B build/movement -DCMAKE_BUILD_TYPE=Release
cmake --build build/movement
ctest --test-dir build/movement --output-on-failure
```

`pet_movement_tests`는8방향64행동·보행 phase·방향 전환·pause/복원을, `pet_camera_tests`는363개 합성 viewport를 독립 crop 기준과 비교한다. 원작 패키지·Windows 결과와 합성 fixture 결과는 구분한다.

## 실제 Windows 검사

MSVC/CMake/Python이 준비된 저장소 루트에서 실행한다.

```powershell
pwsh -File tools/run_windows_checks.ps1
```

이미 빌드한 실행물의 이동 검사만 재현할 때는 다음을 사용한다.

```powershell
pwsh -File tests/movement_windows_tests.ps1 -Exe build/windows/Release/MysteryDungeonPet.exe -Tests build/windows/Release/pet_movement_tests.exe -Output build/windows/movement-results
```

검사기는 정상 MDES5 입력을 매번 생성하며 일반 앱의 탐험·저장·WIC·Animator/Motion·합성·UpdateLayeredWindow를 실행한다. 고정 전환 경로의 첫 방향 전환5.2초를 포함하고 계단 도착6.8초 전까지 관찰한다. 중단/복원 전후 상태도 JSON으로 남긴다. 실행 세션이 잠겨 화면을 관측하지 못하면 결과는pending이다.

## 기록 형식과 재판정

`--movement-trace <csv 경로>`를 명시하면 프레임별 world/raster/camera·clip/frame·이동 action/시각·alpha/hash·화면 제출 성공을 기록한다. 같은 이름의 `-first.bmp`, `-middle.bmp`, `-last.bmp`는 실제 제출 DIB다. 기본1초 성능CSV와 별도이며 진단 기록의 실행 비용을 최종 성능으로 사용하지 않는다.

보존된 [원본CSV·선택PNG·중단 전후 결과](validation/2026-10-15-movement-frames/)의 SHA256은 검증 요약에 있다. 재판정 예:

```sh
python tools/check_movement_trace.py docs/validation/2026-10-15-movement-frames/turn-3.csv --kind turn --scale 3 --output build/movement/turn-3-review.json
```

이 판정은 중간 위치·연결된 행동·방향·camera·alpha·실제 제출을 확인한다. 시각적 부드러움 전체나 사용자의 실제 Windows11 환경을 자동으로 통과시키지 않는다.
