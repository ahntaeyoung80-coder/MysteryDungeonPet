# MDES4 저장 형식

2026-10-14 즉시 구현 명세부터 새 저장은 schema4다. bytes의 필드 순서·크기·CRC32는 [MDES3](MDES3_FORMAT.md)와 동일하다. MDES1·2·3 읽기를 유지하고, 이전 EXE는 MDES4를 읽지 못하므로 pull한 최신 EXE를 사용한다.

## 추가 enum과 단계

- GoalKind의 기존 번호0~4를 유지하고 Auxiliary5를 추가한다. 일반 목표가 없을 때만 비방문 인접 영역으로 최대2행동 경로를 선택하며, 발견 없는 보조 이동은8행동에서 중단한다.
- SceneBeat의 기존 번호0~12를 유지하고 FloorFadeOut13, FloorFadeIn14, WakeDelay15, RoomExit16을 추가한다.
- FloorFadeOut은 계단 도착 위치·Explore·목표 없음으로 저장한다.1000ms 마감 후 기존 stairs onceKey와 새 층 생성/배치를 한 번에 commit한다. 새 상태는 시작 위치·이동 표시 초기화·목표 없음·FloorFadeIn이다.1000ms 후 연출 단계만 해제하고 다음 tick부터 탐험한다.
- WakeDelay는 수면 마감 후 밤 배경/기존 수면 몸체를2000ms 유지한다. Waking에서 낮 배경과 주인공 Wake14를 재생하고, 주인공 완료만으로 RoomExit에 진입한다.
- RoomExit은 기존 Walk0을1200ms 재생하면서 주인공을 방 아래로 이동시킨다. 마감과 주인공 이탈 완료를 모두 확인한 뒤 다음날 생성·HP 회복을 한 번 commit한다. 추가 아침 Nod·fade·동료 이벤트는 없다.

## 목표와 중복 방지

아이템 → frontier → 발견된 계단 순서다. 진행 중인 목표와 대상 instance는 보존하고, 점유 변화는 같은 목표의 경로만 다시 계산한다. 동일 목표 타일의 실패3회는 failedTargets에 저장되어 재선택을 막는다. 계단 경로에서 새 영역이 발견되면 우선 목표를 재평가한다. 목표 경로 중 계단을 우연히 밟아 먼저 층을 바꾸지 않도록 일반 탐험 경로는 계단 타일을 통과하지 않는다.

층 fade·생활·방 이탈 동안 탐험/전투/목표 선택을 실행하지 않는다. 저장은 기존 durable commit 후 표시 방식을 유지한다. 식사 transaction·층 onceKey·다음날 초기화를 재실행으로 중복 적용하지 않는다. 취침 UTC 마감은 유지하며 그 외 연출과 이동의 일시 정지 시계는 함께 보정한다.

기상 후 수집 inventory와 해당 pickup onceKey를 보존한다. 이전 run의 pickup은 해당 inventory 항목과 함께만 허용하고, 새 run의 배우·층 journal 검사는 그대로 유지한다. 현재 원작 자산 입력 및 경로는 변경하지 않는다.
