# 2026-10-21 — 다양성 P0 상태·저장 구현

## 요청·범위·시간

사용자가 `docs/plans/2026-10-21-event-variety-implementation.md` 확인 후 작업 착수를 지시했다. 계획의 첫 단계 P0만 구현·검증했다. P1~P8을 한 번에 변경하지 않았다. 초기 checkout은 main의 문서 상태를 가리켰으므로 `origin/work`를 명시 fetch하고 clean 로컬 work를 `9520643`으로 fast-forward한 뒤 최신 AGENTS/PROJECT_STATE를 다시 읽었다. 원격 제품 코드를 새로 구현하지 않고 재사용했다.

착수 전 P0 총30~50분, 세부 구현20~30분·검사/EXE10~20분을 보고했다. 관련 저장 검사부터 실행하고, 저장/공통 동작 변경 때문에 ASan/UBSan 통합 검사는 마지막에 한 번 수행한다고 알렸다. push가 a2.yml의 전체 Linux/Windows CI를 실행하고 work에는 party 전용 CI가 추가 실행되지 않는 것을 먼저 확인했다.

실측 시작은 09:54:48 KST, 소스/EXE 검사·근거 정리 시각은 10:07:51 KST이며 **13분3초**다. 초기 환경 대기·첫 문서 조회·원격 fetch와 빌드 도구 준비 일부는 실측 시작 전이라 별도 미계측이다. 도구 설치·구현·검사·문서는 병행되어 순수 항목별 시간을 임의로 분리하지 않는다. 최종 commit/push·CI 상태는 아래 마감 기록으로 구분한다.

## 변경

- MDES12는 기존 MDES11 뒤에 길이가 있는 variety block을 추가한다. 구형1~11/MDPE1을 계속 읽고 전체 CRC·검증·원자적 교체 저장을 유지한다. legacy fixture 생성기에서도 새 block을 실제 제거한 후 구형 payload를 만든다.
- event/dialogue/schedule RNG는 고정 salt와 정수 혼합으로 분리하고 저장한다. 대사 추첨은 지도·출현·전투 RNG를 소비하지 않는다. 구형 저장은 mapSeed/runId에서 파생하며 획득 이력으로 열매를 복구하지 않는다.
- 사건 identity·단계/변형·배우 참조·예약, dialogue catalogue/묶음/문장/화자/읽기 상태, 최근 상황·쿨다운·결과 journal·하루 일정을 보관하고 교차 참조/상한/enum을 검사한다. catalogue 불일치는 세션·미소비 예약을 취소하고 확정 결과는 유지한다. 알 수 없는 사건/배우·손상/잘림은 거부한다.
- 평시 사건 보류는 이동/AI/추첨/행동권을 진행하지 않는다. 이미 시작한 전투·생존 회복·필수 scene은 우선한다. 사건/대화 기한, Win32 deadline 예약·pause 이동·캐릭터 변경 보류를 연결했다. 취소는 stale 목표를 지우고 탐험 정책으로 재탐색한다.
- 일반 시작은 기존 길드 아침을 유지하며 대화/사건을 포기하고 예약/배우 참조를 해제한다. 같은 날 일정/목적지·확정 결과·RNG는 새 Explorer에도 넘긴다. 하루 식사는 run이 달라져도 누적 횟수를 중복 증가시키지 않는다. 다음 날 일정·식사 표식만 초기화한다. 같은 날 목적지를 고정했다면 pending package 교체도 다음 날까지 보류한다.
- 새 자동 사건·실제 배우·한국어 대사·휴일·경제는 활성화하지 않았다. 예약은 현재 oran을 참조할 뿐 소비·보상을 적용하지 않는다. 임시 배우/진영은 P5/P6, catalogue 변환·기존 문구 이관은 P2다. 기본 탐험 외 목적지 값은 저장 계약만 준비했다.
- 새 static x64 EXE와 windows-runtime.json을 함께 갱신했다. 원작 그래픽/자산/MDPE는 변경하지 않았다. [상태 계약](../EVENT_VARIETY_STATE.md), [계획](../plans/2026-10-21-event-variety-implementation.md).

## 검증·실패 구분

1. 초기 관련 저장/전투/탐험7개가 통과했다. 새 전용 테스트의 최초 실패는 수동 fixture가 `item:` 이력을 사용한 오류였다. 실제 계약인 `pickup:` 과거 획득 이력으로 고쳐 이행 검사를 통과했다. 제품 로더 검증을 약화하지 않았다.
2. 최종 관련 `pet_variety_state_tests`/`pet_package_transition_tests` **2/2 통과**. 고정 RNG 벡터·실제 탐험 비간섭, 동일 frame100회 보류, pause/deadline, 원자적 파일 결과 전후 재시도·실패 저장의 기존 파일 보존, catalogue 변경3종, 같은 날/다음 날/식사 once·pending package 보류, 잘림·필드/actor/enum/길이/교차 참조 오류·MDES9~11 이행을 확인했다.
3. 저장/공통 행동 변경을 이유로 선택한 GNU14.2 Debug ASan/UBSan 전체 **18/18 통과,95.58초**. 원작 MDPE·구형1~8·동료34개 세부 회귀·탐험10,000지도 등 기존 target을 포함한다. 통과 후 문서/근거만 정리했고 같은 전체 검사를 반복하지 않았다.
4. MinGW GCC14.2/현재 mingw-w64 headers static x64 Release를 빌드했다. Wine10에서 Windows 형식의 새 상태 검사와 원작 패키지 이행 검사2개가 통과했다. 패키지 검사는358회 복원 결정을 비교했다. 최초 host apt는 권한이 없어 실패했고, Docker 안에 도구를 준비했다. 오래된 Bookworm header가 기존 WTSInfoEx 선언을 제공하지 않아 처음 EXE 빌드가 실패했다. compiler와 mingw-w64 dev/common을 trixie 버전으로 맞춰 해결했으며 제품 코드를 우회 수정하지 않았다. 최초 Wine prefix 초기화에는 X display/runtime DLL이 없어 실행되지 않았고 Xvfb·실제 MinGW DLL 경로를 준비한 실행에서 통과했다.
5. 새 EXE의 실제 창을 Wine10/Xvfb에서 실행했다. 기본 길드 아침→탐험은885 trace/541제출/12개 플레이어 타일, 명시 복원은388 trace/193제출/8개 타일이다. 인수 없는 재시작도 길드 아침에서 시작했다. 일반/명시 실행 종료 저장의 schema12·run/day/meal/oran·일정/목적지15·독립 RNG3개가 같고 life만 Wake/Explore임을 확인했다. test-only WTS unlock와 NanumGothic 글꼴 대체를 썼다. 기존 최종 화면2장을 육안 확인했다.
6. 루트 EXE/source/assets 해시 검사는 통과했다. [근거 요약](../validation/2026-10-21-event-variety-p0/summary.json), 관련/ASan/Wine 로그·스크린샷·GUI 실행 helper를 같은 폴더에 보관했다. 신규 사건의 상세 상태는 core 강제 fixture에서 검증했고 실제 GUI에 자동 등록한 것으로 보고하지 않는다.

## 남은 범위·재개

P0 검증을 마쳤다. 다음은 P1에서 기본 던전15의 실제 출현 MD별 필수 몸체/Normal 얼굴을 패키지화하고 표시 종을 일치시키는 작업이다. 빈도·교환·보상·휴일 주기는 미정이다. 실제 사용자 Windows11·OS lock/power·장기/최종 성능·A/C의 잔여 gate는 pending이다. 자동 전체 CI는 push 후 백그라운드로 확인하며 아직 성공으로 포함하지 않는다.

마감 `git diff --cached --check`가 Windows 로그의 CRLF를 공백 오류로 검출해 근거 텍스트의 줄바꿈만 LF로 정규화했다. 제품 소스/EXE는 바뀌지 않았다.
