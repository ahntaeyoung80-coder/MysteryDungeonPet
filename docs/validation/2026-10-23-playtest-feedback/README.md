# 플레이테스트 수정 검증 자료

`summary.json`은 고유 검사 수와 최초 실패/중단·표적 재검사, 화면과 설치물의 범위를 구분한다. 전체 검사는 반복하지 않았다. 원본 사용자 로그는 저장소에 올리지 않는다.

- stage1: 긴 원정의 누적 획득 기록을 가진 짧은 원정 전환과 잘못된 키 거부, 64개 조합.
- stage2/3/4: 밸런스·일정·이름·문구·화자·한 줄 패널의 관련 검사.
- final-core/final-survival-retest: 31개 최초 통과와 중단한 구 급소 검사 1개의 보정 후 통과.
- final-python/final-python-retest: 49개 최초 통과와 빌드/검사 겹침으로 접근 오류였던 1개의 통과.
- final-windows-history/final-windows-gdi: Windows x64 실행 파일로 확인한 누적 이력 전환·한글 glyph·긴 이름과 불투명 잉크.
- stage4-gui/entry-summary, final-installed-gui: 단계별 기본 화면 근거. final-pagination-gui와 final-launch-gui는 최종 단어 경계·재실행 새 원정 정책의 표적 재검사다. 초기 글꼴 설정이 없는 실행은 성공한 한글 화면으로 계산하지 않는다.
- install-summary: build/root/install EXE와 선택 자산 바이트 일치. native Windows 실행 통과를 뜻하지 않는다.

## 검사 재현

```sh
cmake -S . -B build/playtest-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -O1"
cmake --build build/playtest-asan --parallel 3
ctest --test-dir build/playtest-asan --output-on-failure
python -m pip install -r requirements-offline.txt
PET_PACKAGE_INSPECT="$PWD/build/playtest-asan/pet_package_inspect" PET_EXPLORE="$PWD/build/playtest-asan/pet_explore" python -m unittest discover -s tests -v
```

빌드가 끝난 뒤 검사를 시작한다. Windows에서는 기존 `tools/run_windows_checks.ps1`을 사용한다.

Wine 화면 검사에는 별도의 `WINEPREFIX`, Xvfb, 한글 글꼴, 빌드한 Windows EXE가 필요하다. `window_probe.cpp`를 Windows용 C++ 컴파일러로 static 빌드하고 `verify_ui_wine.py --help`의 경로를 지정한다. 이 스크립트는 명시적으로 합성 WTS 잠금 해제 메시지를 사용한다. `--duration 24`로 실행하면 기상 후 원정 입장까지 관측할 수 있다. 기본 실행의 재실행은 현재와 다른 원정을 선택하지만 날짜·재고는 유지한다. fixture는 기존 `pet_variety_scenario`로 생성하며 강제 조건과 자연 플레이 전체 승리를 구별한다.
