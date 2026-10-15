# MDES6 — 탐험 반복 감지

MDES5의 모든 필드를 같은 순서로 보존하고 survivalFirst 뒤, CRC32 앞에 다음을 추가한다. 기존 MDES1~5 reader를 유지하며 이전 저장은 빈 navigationHistory로 시작한다.

- u32 navigationHistory count (0~32)
- count개의 u64 joint-state fingerprint (오래된 것부터)

Fingerprint는 FNV-1a64로 층·지도/RNG·플레이어/배우 위치·목표·방문/발견/실패/차단 mask·소지/완료 기록 수·HP·열매·추적 상태·현재 행동권을 포함한다. 절대 시각·round 번호·history 자체를 제외해 같은 진행 상태의 재등장을 감지한다. 진행으로 방문/발견/획득/목표 등이 바뀌면 다른 상태다. 동일 상태가32개 기록 안에서 재등장하면 같은 층의 합법 인접 칸으로 우회한다. 기존 다음 칸과 직전 칸을 가능하면 피하고 같은 fingerprint의 누적 횟수로 후보를 순환한다. 후보를 한 바퀴 고르면 해당 fingerprint는 최신1개만 남겨 고정 길이 history의 포화로 같은 방향이 고정되지 않게 한다. 막혀 기다릴 때는 목표 재계획 후 상태도 기록한다. 정상 층 진입/수동 Reset에서 history를 비운다. 바이트 배치는 그대로이며 기존 MDES6 파일을 읽는다. 이전 자동 귀가 정책은 사용자 정정으로 폐기했다.

수동 resetExploration만 현재 탐험/전투/대화를 중단하고 기존 ReturnHome 트랜잭션으로 연결한다. inventory와 확정 HP/완료 journal을 보존하고 이동/추적을 지우며 아직 적용되지 않은 pending-hit만 취소한다. clear/pickup을 새로 지급하지 않는다. manual-reset:<runId>와 return:<runId>가 저장돼 재호출·복원에서 귀가를 반복하지 않는다.
