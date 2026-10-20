# 2026-10-20 — 대화 초상화와 탐험 중 동료 대화

## 범위·예상·시작점

사용자는 우호 NPC 대화에 주인공/상대 초상화를 원작처럼 텍스트박스 위에 표시하고, 취침·기상 대화에도 초상화를 추가하며, 탐험 도중 가끔 뒤를 돌아 동료와 대화하도록 요청했다. 사전 예상은 소스/자산 확인5~10분, 구현15~25분, 관련 검사와 실제 실행10~15분이었다. 최초 로컬 work는 main 문서 상태468afc4여서 그대로 개발하지 않았다. 명시 fetch는 CLI 인증에서 막혔고 연결된 GitHub API로 실제 work7ae3bf3와 로컬 Git object를 대조해 그 커밋에서 작업했다. 최초 시계 기록은 UTC07:21:07이며 그 전 브랜치 확인 구간은 분리 계측하지 않았다.

## 변경

- 기존 원작 대화창·순차 텍스트 출력 위에 원본40×40px 초상화를 합성한다. 주인공/우호 켈리몬, 주인공/동료, 기상 알람의 노공룡을 장면별로 선택하고 발화자 테두리를 강조한다. 던전 카메라나 확대 배율에 영향을 받지 않으며 텍스트 영역을 덮지 않는다.
- 제공 ROM의 KAO0/3/373/321을 직접 확인해 추출했다. MD383의 초상화를 MD-1로 추정하면 다른 종이 나오므로 명시 매핑과 출처 해시를 남겼다. 고정 SkyTemple 커밋5ed359a로 재추출해 전체 패키지·PNG·메타데이터 동일성과 ROM 불변을 확인했다.
- 탐험 중 안전한 인접 경계에서 주인공이 동료를 보고, 동료도 주인공을 본다. 두 발화는 각1600ms와 해당 배우의 Nod 완료를 기다린다. 첫20~28걸음 후32걸음 구간마다 한 번 기회를 주며 적·저체력·기절·계단·미완료 행동/이동에서는 미룬다. 대화 중 이동/전투를 진행하지 않고 완료 후 목표·라운드·RNG를 보존해 탐험을 잇는다.
- 기존 NPC 대화는 각1200ms를 유지하고 아침 인사에는 최소1600ms 읽기 시간을 확보했다. 동료가 NPC 대화에서 함께 고개를 끄덕이던 동작도 자신의 발화에만 Nod하도록 구분했다. MDES9 배치를 바꾸지 않고 기존 actor ID와 once journal을 사용한다. 동료 대화 ID의 명시 범위 검사·완료 소유자·복원·수동 귀가 시 정리를 추가했다.

## 검사·실패 수정·실제 실행

관련 Linux 검사5개(dialogue portrait, entry/social scene, gameplay, companion, survival)가 통과했다. 신규 검사는 발화자 전환/초상화 배치·clipping, 안전/위험 조건, 연속 이동 경계에서 대화 시작, 실제 완료 소유자, 대화 도중 저장 복원과 목표 재개를 다룬다. 첫 실행에서는 새 동료 대상 ID가 저장 로더의 기존 NPC 범위에 걸린 점과 불필요하게 바꾼 NPC 시간이 기존 회귀에 걸린 점을 발견했다. 저장 허용 범위를 명시하고 NPC1200ms를 복원한 후 해당 검사부터 재실행했다. 잘못된 테스트 fixture의 action 시각과 NPC goal도 바로잡았다.

MinGW GCC14.2 static x64 새 PE를 Wine10/Xvfb에서 실행했다. NPC 양쪽 발화·동료 양쪽 발화·취침 인사·아침 인사·기상 알람의7개 캡처에서 원본 초상화40×40픽셀 일치·발화자 강조·텍스트박스 alpha255를 확인했다. NPC/동료 대화 중 위치·라운드·이동 정지와 이후 이동 재개를 CSV로 확인했다. 실제 GDI 한글7장면도 통과했다. Wine의 WTS 조회 제한은 외부 unlock 도우미로, 글꼴은 Wine registry에서만 Malgun Gothic→NanumGothic으로 처리했고 제품의 Windows 정책/글꼴 설정은 유지했다. native Windows/사용자 Windows11 검증은 pending이다.

작은 대화 연출 변경이므로 전체 메모리/전체 CI를 배포 조건으로 추가하지 않았다. push마다 실행되는 a2.yml과 별도 브랜치 전용 workflow를 확인했으며 소스·자산·새 EXE를 한 번에 work에 반영해 중복 push CI를 피한다. 자동 CI는 배포 후 백그라운드다. EXE와 소스/자산 hash 검사는 통과했고 기록만 추가했다는 이유로 같은 코드 전체 검사를 반복하지 않았다.

[검증 요약과 원본 대조](../validation/2026-10-20-dialogue-portraits/summary.json), 같은 폴더의7개 PNG와 portrait-source-audit.json을 참고한다. 재현은 `cmake --build build/<설정> --target pet_dialogue_portrait_tests pet_entry_social_scene_tests pet_gameplay_tests pet_companion_tests pet_survival_tests`, 해당5개 CTest, `python -m tools.prepare_dialogue_portraits --out <새 경로>`로 수행한다. GUI fixture는 `pet_dialogue_portrait_tests --fixtures <exploration.mdpe> <출력>` 및 기존 entry/social의 `--bed-fixture`/`--wake-fixture`를 사용하고 EXE의 `--movement-trace`로 기록한다.

UTC07:34:41까지 계측 구간은 **13분34초**이며 도구 준비·구현·검사·기록을 병행했다. 항목별 순수 시간과 도구 대기를 따로 계측하지 않았고 초기 소스 확인 구간은 이 수치에 포함하지 않는다. UTC07:36:34의 배포 준비까지는 **15분27초**다. 이후 원격 반영/확인 구간은 이 기록에서 분리 계측하지 않는다. 이 기록을 포함한 배포 커밋으로 원격 work 갱신과 SHA 대조를 수행한다. 다음 확인은 사용자 Windows11의 실제 표시와 남은 B 인수 기준이다.
