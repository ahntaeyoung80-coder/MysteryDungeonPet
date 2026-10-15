# MDES8 — 배우별 행동·재생 완료

3단계 구현 형식. MDES7 prefix와 기존 enum을 유지하고 CRC 앞에 독립 파티 진행을 추가한다. 읽기는 MDES1~8, 쓰기는 MDES8이다. 이 문서만으로 구현/검증 완료를 뜻하지 않으며 결과는 [3단계 로그](../logs/2026-10-15-party-ai-stage3.md)를 따른다.

## 추가 payload

모든 정수는 기존 MDES와 같은 little-endian이다. bool은0/1의 u32다.

| 순서 | 타입 | 의미 |
| --- | --- | --- |
| 1 | u32 | independentParty |
| 2 | u64 | 다음 액션/재생 ID,1 이상 |
| 3 | u32 | 라운드 단계:0 Idle,1 Allies,2 Enemies |
| 4 | u32 | 동료 의도:0 Follow,1 Engage,2 Return |
| 5 | u32 | 동료가 마무리할 적 instanceId, 없으면0 |
| 6 | u32 | PartyAction 개수, 최대 kTiles+2 |
| 7 | count×60bytes | 아래 액션 |
| 8 | u32 | PartyPlayback 개수, 최대 kTiles+2 |
| 9 | count×44bytes | 아래 재생 |
| 10 | u32 | 이 payload의 byte 길이(이 길이 필드와 CRC 제외) |
| 11 | u32 | 전체 파일의 기존 CRC32 |

고정 payload32bytes + 액션60bytes×개수 + 재생44bytes×개수다. 길이 필드를 포함하면 MDES7 대비36bytes + 가변 records가 늘어난다.

액션은 id/round/startedMs/deadlineMs(u64×4), actorId/targetId/damage/kind(u32×4), hit/critical/finished(u32×3) 순서다. kind는0 Wait,1 Move,2 Attack,3 Berry다.

재생은 id/round/startedMs/deadlineMs(u64×4), actorId/clip/direction(u32×3) 순서다. clip은 Attack1/Hurt6/Faint33이다. 이동은 기존 MoveRecord/Animator를 재사용한다.

actorId0은 주인공,4294967294는 동료, 나머지는 적 instanceId다. 완료 입력은 runId/floorIndex/round/actorId/actionId 전부를 해당 재생과 대조한다. 다른 배우·다른 액션·다른 탐험/층의 완료는 무시한다.

## 확정·복원 규칙

- 각 배우의 기존 actionRound와 현재 PartyAction을 함께 확정한다. 이동·공격·열매·대기는 한 라운드의 한 행동이다.
- 공격 시작에 대상·명중/급소·피해·RNG 진행을 저장하고 공개한다. 해당 배우의 공격 clip 완료 또는 유한 timeout에서 피해를1회 적용한다.
- party-attack 키는 run/floor/round/actor, party-impact 키는 run/floor/round/actor/action으로 구분한다. finished와 impact journal이 불일치하면 거부한다.
- 공격 완료와 피해 대상의 Hurt/Faint 완료를 분리한다. 한 적의 기절 재생은 그 적을 제거하며 다른 적/아군의 행동을 삭제하지 않는다.
- 동료의 전투·복귀 위치는1칸 밖에서도 저장한다. 지형·모서리·배우 비중첩을 계속 검증한다.
- 일시정지는 현재 액션·재생의 시작/기한을 이동시킨다. 수동 Reset은 이미 확정한 HP/획득을 보존하고 미확정 액션을 취소한다.
- actor/id 중복, 없는 대상, 잘못된 결과/회복 수치, 액션과 행동권 불일치, 소유 공격의 재생 유실, 개수/길이/CRC 오류를 거부한다.

## 이전 저장 이행

MDES1~7의 새 필드는 기본 비활성·빈 진행이다. 이전 단일 조우의 enemyInstance/hit/damage/공격 대상·pending-hit/RNG는 기존 reader와 전투 경로로 보존한다. 진행 중 전투를 Explore로 강제 초기화하지 않는다.

구형 조우를 마친 뒤 Explore에서 enableCompanion이 독립 파티 방식을 활성화한다. 따라서 이전 저장의 아군 공격이 공격 시작 때 이미 확정됐거나 적 피해가 완료 대기 중이어도 다시 적용/추첨하지 않는다. 생활 장면의 진행도 기존 경로를 유지한다.

구형 fixture 생성은 tests/legacy_snapshot.hpp의 removePartyBlock으로 MDES8 payload와 길이를 제거한 뒤 MDES7의 고정76bytes 및 종전 필드를 제거하고 CRC를 다시 계산한다. 버전 숫자만 바꿔 다른 구조로 읽히게 하지 않는다.
