# 최소 B-2 전투 ID와 구현 계약

2026-10-13. 최초 main/origin/work 어디에도 이 파일은 없었다. 이번 사용자 확정 ID·규칙과 보관된 실제 WAN 시트 검사에 근거해 작성한다. 원작 능력치 계산의 완전 이식 문서가 아니다.

| 대상 | ID | 확인·사용 범위 |
| --- | --- | --- |
| 기본공격 | MOVE_REGULAR_ATTACK355 | 사용자 확정 계약; 이번 런타임의 유일한 공격 |
| 오랭열매 | item70 | 사용자 확정 계약; 소지1·회복2·상한3·1턴 소비 |
| 공격 몸체 | WAN1/Attack | 보관된 MD1/MD4 AnimData.xml 실제 Index와 raw WAN metadata 확인 |
| 피격 몸체 | WAN6/Hurt | 같은 두 종에서 지원; 유한 재생 |
| 기절 몸체 | WAN33/Faint | 적 카운터0 후 유한 재생 완료를 기다려 승리 |
| 바닥 충돌 | WAN34/HitGround | 실제 지원 확인·패키징; 현재 별도 재생 필요 없어 사용 보류 |

공격 계산은 신규 최소 규칙이다. 플레이어·적 동일80% 명중 판정, 명중한 공격의 치명타20%/5%, 일반1·치명타2다. 적은 HP UI 없이 잔여 적중 카운터2, 플레이어는3하트다. 난수 상태와 피해 결과를 같은 MDES2 snapshot에 저장하므로 복원 뒤 다시 추첨·적용하지 않는다.

`HostileEncounter → PlayerTurn → PlayerAttack/UseOranBerry → EnemyTurn → EnemyAttack → PlayerTurn`. 공격 clip 완료와 Hurt clip을 분리하고, 적 Faint 완료 뒤 CombatVictory로 전환한다. 승리 시 귀가하지 않는다. 사망 시 CombatDefeat에서 공통 ReturnHome→감정표현→Eat→Sleep→Wake 파이프라인을 사용한다.

열매는 Idle 위 텍스트만 출력하며 사용 동작은 없다. 기술·레벨·선택 UI·위력/타입/상태/사거리/PP·복수 가방·복잡한 원작 능력치·적 HP 표시는 보류한다. 실행 자산과 원본 hash는 `assets/original/gameplay/provenance.json`, 검증은 `docs/B1_GAMEPLAY_TASK.md`에 있다.
