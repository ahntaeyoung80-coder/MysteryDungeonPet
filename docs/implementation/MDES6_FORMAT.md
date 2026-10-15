# MDES6 — 탐험 반복 감지

MDES5의 모든 필드를 같은 순서로 보존하고 survivalFirst 뒤, CRC32 앞에 다음을 추가한다. 기존 MDES1~5 reader를 유지하며 이전 저장은 빈 navigationHistory로 시작한다.

- u32 navigationHistory count (0~32)
- count개의 u64 joint-state fingerprint (오래된 것부터)

Fingerprint는 FNV-1a64로 층·지도/RNG·플레이어/배우 위치·목표·방문/발견/실패/차단 mask·소지/완료 기록 수·HP·열매·추적 상태·현재 행동권을 포함한다. 절대 시각·round 번호·history 자체를 제외해 같은 진행 상태의 재등장을 감지한다. 진행으로 방문/발견/획득/목표 등이 바뀌면 다른 상태다. 동일 상태가32개 기록 안에서 재등장하면 한 번 안전 귀가하며 정상 층 진입/Reset에서 history를 비운다.

resetExploration은 현재 탐험/전투/대화를 중단하고 기존 ReturnHome 트랜잭션으로 연결한다. inventory와 확정 HP/완료 journal을 보존하고 이동/추적을 지우며 아직 적용되지 않은 pending-hit만 취소한다. clear/pickup을 새로 지급하지 않는다. navigation-reset:<runId> 또는 manual-reset:<runId>와 return:<runId>가 저장돼 재호출·복원에서 귀가를 반복하지 않는다.
