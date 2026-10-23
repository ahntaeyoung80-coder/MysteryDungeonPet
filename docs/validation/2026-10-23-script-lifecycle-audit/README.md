# 대본·생활 흐름 감사 검증 자료

2026-10-23 KST. 최종 제품 소스는 `08504b7`, 대본 반영은 `a9eafa6`, 문서 선행 완성은 `6454693`이다. [최종 수치](summary.json), [상세 작업·시간](../../logs/2026-10-23-script-lifecycle-audit.md), [흐름별 감사](../../dialogue/LIFECYCLE_AUDIT.md)를 참조한다.

## 재현

Linux에서 cmake/ninja와 C++17 도구를 준비한다.

```sh
python -m tools.compile_dialogue_catalogue
python -m tools.compile_item_comments
python -m unittest discover -s tests -p 'test_dialogue_catalogue.py' -v
python -m unittest discover -s tests -p 'test_item_comments.py' -v
cmake -S . -B build/audit -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/audit --target pet_lifecycle_audit_tests --parallel 3
build/audit/pet_lifecycle_audit_tests
```

ASan/UBSan은 별도 build/audit-asan에 `-DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'`로 구성하고 같은 회귀만 실행했다. 제품 소스가 같은 통과 검사를 문서 기록 때문에 반복하지 않았다.

Windows EXE는 Debian trixie의 MinGW14.2로 빌드했다. bookworm의 이전 헤더에는 WTSInfoEx가 없어 실패했다. 최신 컨테이너에서 다음을 사용했다.

```sh
cmake -S . -B build/audit-win-current -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS=-static
cmake --build build/audit-win-current --target MysteryDungeonPet pet_lifecycle_audit_tests pet_text_overlay_tests --parallel 3
cmake --install build/audit-win-current --prefix build/audit-install
```

Wine10 GUI 재현은 소스를 `/project`로 마운트한 컨테이너에서 수행했다. Linux fixture 도구의 `__FILE__` 절대 경로를 유지하도록 `/workspace/MysteryDungeonPet`를 `/project`에 연결했다. `run_wine_gui.py`는 이 경로와 `build/audit`, `build/audit-win-current`를 사용한다. `window-probe.cpp`를 `build/window-probe.exe`로 정적 링크하고 x64 Wine과 Xvfb를 준비한다.

```sh
x86_64-w64-mingw32-g++ -static docs/validation/2026-10-23-script-lifecycle-audit/window-probe.cpp -o build/window-probe.exe -luser32
xvfb-run -a python3 docs/validation/2026-10-23-script-lifecycle-audit/run_wine_gui.py
```

검사 전 Wine prefix `/tmp/mdpet-audit-wine`에 NanumGothic을 설치하고 Malgun Gothic 대체 글꼴로 등록했다. 제품 글꼴 설정이나 설치물에 검사용 폰트를 추가하지 않았다. helper는 WTS_SESSION_UNLOCK을 합성하므로 실제 OS 잠금 검사가 아니다.

## 결과 읽기

- stage2: Python10개, 코어4개, PE2개, 한글 GUI2건. alarm.png/morning.png에서 완성 대사와 실제 화자 초상화를 확인했다.
- stage3-before.txt: 수정 전5그룹 중4그룹 실패. stage3-after.txt/asan.txt:67검사 통과. 관련14개는 최초12통과·2실패 후 retest.txt의2개를 통과했다. entry-baseline.txt는 변경 전 소스에서도 남아 있던32px 패널 기대값 실패를 기록한다.
- stage3-windows-tests.txt의 첫 GDI 실패와 windows-retest.txt의 최종 통과를 함께 보존한다. 조용한 플레이어 구간은 잉크0개, 다른7장면/층수3개는 실제 GDI 잉크를 확인했다.
- stage3-gui-initial.json은 오래된 시각의 저장 복원 성공이다. stage3-gui.json은 즉시 생성한 저장에서 BedLay부터 확인한 최종2건이며 최종 저장 day2·meals1·탐험 상태를 decoder로 확인했다. next-day.png는 해당 EXE의 다음 날 입장 화면이다.
- summary.json의 installedGui2건은 설치본 첫 실행·기존 저장 재실행이다. build/root/install EXE 및 설치 대사 자산이 동일하다.

실제 Windows11·장기 성능·모든 원정의 자연 전투 승리·원격 CI 통과 결과는 이 자료의 주장 범위가 아니다.
