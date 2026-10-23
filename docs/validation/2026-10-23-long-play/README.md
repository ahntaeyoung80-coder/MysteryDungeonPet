# 장시간 반복 검증 절차

현재 결과는 `summary.json`과 날짜별 작업 로그를 따른다. 아래 실행은 코어의 가상 시각 진행과 실제 EXE의 wall time 관측을 구분한다.

## 코어 반복

```bash
cmake -S . -B build/long-play-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/long-play-release --target pet_long_play_tests -j 2
build/long-play-release/pet_long_play_tests --roots 1,4,8,14 --days 6 --seeds 2 --output build/long-play-extended-a
build/long-play-release/pet_long_play_tests --roots 17,21,35,41 --days 6 --seeds 2 --output build/long-play-extended-b
```

두 실행의 출력 폴더는 별개다. 같은 빌드 폴더의 빌드와 검사를 동시에 실행하지 않는다. CTest 기본 범위는 시작1/14·두 날·seed41·3방식6조합이며 확장 실행은8시작×3방식×2seed=48조합·각6일이다.

- 첫 원정은 지정한 시작 ID를 실제로 걷고 이후 원정은 현재 맵을 제외해 선택한다. 계단 강제 이동·적 처치·체력 보충·임의 귀가를 사용하지 않는다. 승리 또는 생존 철수/패배로 하루를 마친다.
- 방식0은 단독,1은 이전 동행 방식으로 시작,2는 독립 파티다. 방식1도 원래 코드의 사건 전투가 독립 파티로 전환하면 그 결과를 따른다. 새 run 시작 때 방식1만 호환 경로를 다시 선택한다.
- 화면 탐색200px와 논리 위치의 카메라 관찰을 켠다. 실제 GDI·보간 프레임 관측은 아래 EXE 검사가 담당한다.
- 가상 시각을250ms씩 진행한다. seed41은 유한 동작을600ms 뒤 완료한 것으로 알리고 매7번째 통지를 생략하며 낡은 run 토큰도 섞는다. seed99는 완료 통지를 모두 누락시켜 유한 fallback을 검사한다. 실제 애니메이션 길이 성능 측정이 아니다.
- 31결정마다 및 전투/생활·연출 구간에서 snapshot을 읽은 Explorer와 원래 Explorer에 같은 다음 입력을 넣고 결과 bytes·event·onceKey를 대조한 뒤 복원본으로 계속한다. 저장에 없는 다음 원정 정책은 앱과 같이 재설정한다.
- 997결정마다60초 pause clock 이월을 적용하고 RNG·재고가 바뀌지 않는지 확인한다. 실제 OS 절전으로 판정하지 않는다.
- 매 갱신에서0/1소지·체력 상한·날짜 증가·일일 식사·획득 이력 보존을 검사한다. 열매 사용은 같은 갱신의 새 획득까지 더해 정확한 차감을 대조한다. 적 급소·귀가 중복도 거부한다.
- 첫 보스 진입, 준비/친구 대화, 휴일은 실제 관측한 횟수를 기록한다. 모든 조합의 보스 승리를 가정하지 않는다.
- 실패 시 상태와 위치를 기록한다. 유효 상태를 저장할 수 없으면 직전 checkpoint를 별도 남긴다.

ASan/UBSan은 같은 소스의 `--roots 1,14 --days 2 --seeds 2`를 실행한다. 저장·전투 공통 수정에 직접 연결되는 기존 생활·전투·동료 검사도 별도로 실행한다. Windows PE도 같은 코어 검사와 기절 회귀를 Wine에서 확인한다.

## 실제 EXE 관측

```bash
python docs/validation/2026-10-23-long-play/run_wine_soak.py --output build/long-play-gui-final --wine "$WINELOADER" --window-probe /absolute/path/window_probe.exe --duration 900
```

Wine/Xvfb가 준비된 환경에서 실행한다. 최종 결과는 정상 종료·900초 wall time·메트릭 및 이동 trace·마지막 snapshot을 대조한다.60초 이후 메모리의 범위와 마지막 값을 기록하며0만 출력되는 Wine 카운터는 미지원으로 표시한다. Windows11 CPU/RAM 목표 통과나 핸들 누수 없음으로 확대해석하지 않는다. 원시 CSV는 build에 두고 요약과 대표 화면만 저장소에 남긴다.

## 검사 구성 중 확인한 사항

최초 컴파일은 누락한 `exploration_package.hpp` include를 보완했다. 초기 ASan 실행은 화면 관측/진행 진단을 더하기 위해 중단했으며 통과로 집계하지 않았다. 자연 전투의 기절 정체를 먼저 발견해 회귀로 고정했다(`faint-before.txt`).

열매 검사 초기 실패는 동료 사용과 플레이어 새 획득이 같은 갱신에 발생한 경우를 무시한 검사식 문제였다(`initial-berry-assertion.txt`). 제품 수량 정책을 바꾸지 않고 새 획득을 식에 포함했다. 첫 원정을 지정한 ID로 걷는지 확실히 하기 위해 초기 일정을 명시했고 해당 경로부터 다시 검사한다. 초기 Windows12조합/24일 결과는 별도 preliminary이며 최종 시작 정책 검증과 합산해 고유 조합 수를 부풀리지 않는다.

첫 EXE 관측은 기절 수정 EXE로 바꾸기 위해 약7분에 종료했다. 그 관측의 요약기에서 trace 필드명을 잘못 쓴 `KeyError`를 수정했다. 중단/요약 실패 실행은15분 통과로 집계하지 않고 최종 EXE로 새로 관측한다.

## 중단 후 재개

완료 JSONL을 지정하면 통과 조합만 건너뛰고 별도 출력 폴더에 나머지 결과를 쓴다. 중단된 조합은 처음부터 다시 실행하며 미완료 진행 수치를 통과에 합산하지 않는다.

```bash
build/long-play-release/pet_long_play_tests --roots 1,4,8,14 --days 6 --seeds 2 --skip-passed build/long-play-extended-a/cases.jsonl --output build/long-play-resumed-a
build/long-play-release/pet_long_play_tests --roots 17,21,35,41 --days 6 --seeds 2 --skip-passed build/long-play-extended-b/cases.jsonl --output build/long-play-resumed-b
python docs/validation/2026-10-23-long-play/summarize_results.py --output docs/validation/2026-10-23-long-play/summary.json
```

집계기는 Release48조합·각6일, ASan12조합·각2일의 중복·누락·실패를 거부한다. 재개 전 적 급소 검사는 비플레이어 공격 이벤트 전체에 더 강하게 적용했다. 이후 코드를 읽어 우호 NPC도 같은 이벤트명을 쓴다는 점을 확인하고 적대 배우만 검사하도록 정확히 좁혔다. 이미 통과한 더 강한 검사 결과를 유지한다. `attacks`는 반환된 대표 공격 이벤트의 집계이며 한 갱신에 실행된 모든 배우의 공격 수를 뜻하지 않는다.

재개한 시작8·방식2·seed41에서38층 이동 사건의 라운드 불일치가 검출됐다. `movement-reproduction.txt`는 유효 checkpoint에서 첫 이동이 이전 완료 Wait를 남겨 두고1700→1701라운드를 만드는 과정을 기록한다. `movement-before.txt`는 최소 회귀의 실패, `movement-after.txt`는 E02/E06/E08×이전 동작 유무6경로·모든 맵 사건·저장 상태의 최종 통과다. 실패 조합은 수정 후6일 전체를 다시 실행하고, 집계기에 해결 전 실패도 별도 남긴다. 기존288일의 통과 조합을 전부 재실행하지 않고 마지막 이동 수정에 직접 연결되는 경계를 재검사한다.

재개 직후 GUI 시도는 꺼진 X 서버로 `CreateWindow OS=1400`, 다음 시도는 실행 세션 밖에서 종료된 X 서버로 `XIO fatal IO error`가 났다. 제품 탐험 오류로 합산하지 않고 창 서버를 별도 유지한 세션(:96)에서 새15분 관측을 실행한다. 관측기에는 프로세스 종료/빈 지표를 명시적으로 거부하는 처리도 추가했다.
