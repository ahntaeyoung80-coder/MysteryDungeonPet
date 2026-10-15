# MDES5 저장 형식

기준 MDES4의 필드 배치와 enum 번호를 유지한다. Population의 마지막 MoveRecord 뒤, CRC32 앞에 SurvivalFirst(u32)를 추가했다. None0·Oran1·Stairs2이며 새 층에서 초기화한다. 새 저장은 schema5이고 MDES1~4 읽기를 유지한다.

## 생존과 행동

- HP2 이하에서는 일반 아이템/인사보다 생존 목표를 우선한다. 열매를 먼저 발견하면 도달 가능한 오랭열매를 찾고, 없으면 발견한 계단으로 도망간다. 계단을 먼저 발견하면 계단으로 도망간다. 같은 발견 시점에는 열매를 우선한다. 무엇을 먼저 발견했는지는 저장/복원 후에도 유지한다.
- 보유 열매는 HP3 이하(2칸 이상 손실)에서만 사용한다. 탐험에서도 사용하며 한 플레이어 행동을 소비하고 기존 회복량2를 적용한다. 저체력에서는 플레이어가 먼저 전투를 시작하지 않고 안전 경로로 후퇴한다. 적에게 남은 공격권을 없애거나 무적 상태를 부여하지 않는다.
- 아이템 획득도 playerActionRound를 소비하고400ms 마감을 둔다. 재실행·완료 신호로 같은 아이템/턴을 중복 처리하지 않는다.
- 플레이어의 경로에서 우호 NPC 타일은 통과 가능하다. 그 타일로 이동하는 순간 NPC를 플레이어의 이전 타일로 옮기는 원자적 자리 교환을 수행한다. 플레이어/NPC의 반대 방향 MoveRecord를 같은 round·시각·400ms로 저장한다. 적 경로에는 일반 점유 규칙을 유지한다. 교환으로 NPC의 done·인사 onceKey를 바꾸지 않는다.
- SceneBeat의 기존0~16을 유지하고 ExplorationBerryText17을 추가했다. 기존 열매 메시지와1000ms 대기를 탐험에서도 재사용하며 이 동안 추가 탐험 행동을 실행하지 않는다.

## 피격·도주·NPC 대화 보정 (2026-10-14)

바이너리 배치·schema5·enum 번호를 유지한다. EnemyTurn에서 RNG와 공격권·roll을 commit하고 gameplay.onceKeys에 `pending-hit:<run>:<floor>:<enemy>:<round>`를 저장한다. EnemyAttack의 완료 신호/기존 유한 animation timeout 이후에만 이 키를 제거하면서 HP를 차감하고 PlayerHurt로 진행한다. 키가 없는 이전 EnemyAttack 저장은 HP가 이미 반영된 것으로 처리한다. 공격 중 복원해도 RNG/HP를 다시 판정하지 않으며 완료 후 복원해도 HP가 두 번 차감되지 않는다.

저체력에서 적의 잔여 이동과 다음 플레이어 이동을 한 snapshot에 commit하고 같은400ms에 표시한다. 적의 공격권은 유지하고, 실제 피격 연출 이후에만 필요한 정지가 생긴다. 길의 미인사 우호 NPC가 다음 경로 타일을 차지하면 Player/Npc 대화를 순서대로 재생한다. item/stairs 목표는 유지하며 인사 후 첫 이동으로 자리 교환한다. 인사 자체가 추가 이동 턴을 소비하지 않는다. onceKey와 done으로 같은 NPC 인사를 반복하지 않는다.

## 취침·기상·방 이탈

동료와 밤 침실에서 마주보는 Nod22(BedFace3) → Laying27(BedLay4, 최소400ms) → EventSleep13(Sleeping7, 두 배우가 한 번 재생) →750ms BedFadeOut5 → 낮 배경750ms BedFadeIn12(Wake) → 두 배우 Wake14(Waking8) → 아침 Nod22(MorningFace9) → 주인공 Walk0(RoomExit16) → 다음날 갱신이다. 수면 정지10초와 WakeDelay2초는 새 경로에서 실행하지 않는다. fade-out에는 잠들기 마지막 프레임을 유지한다. 원작에 없는 Laying 전환 프레임을 만들지 않으며 동료의 별도 퇴장도 없다.

퇴실은 기존 침상(204,184) → 침상 가장자리(184,200) → 동료 침상 아래(128,200) → 통로(112,184) → 왼쪽 통로 입구(104,184)다. 총1870ms이며 동료 침상과 맵 경계를 넘지 않는다. 방향·카메라는 공통 경로 표본을 사용한다. UTC phaseStartedMs로 남은 구간을 복원하고 pause 시 finite animation 시계를 보정한다. 모든 schema의 RoomExit 복원 마감은 phaseStartedMs+1870ms로 바꿔 이전 긴 경로도 새 경로로 끝낸다. 퇴실 완료 후에만 HP·새 run/RNG·탐험을 한 번 갱신한다.

이전 발견 순서가 없으면 알려진 계단, 없으면 알려진 열매를 우선한다. 새 pending-hit 저장은 갱신된 EXE로 열어야 한다. 원본 자산·배경·cue 파일은 변경하지 않는다.
