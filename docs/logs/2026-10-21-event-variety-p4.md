# 2026-10-21 — P4 맵·아이템·결과

사전 범위: 원작 새 맵·종결 장소·shop/MH/trap 문맥·현재 재고/예약/나눔. 예상40~60분. 측정 2026-10-21T03:52:58+00:00→2026-10-21T04:05:37.963446+00:00 (760초). 자료추적·추출·코드·간단검사가 병행되어 항목별 실측은 분리하지 않는다. 선행 P3 로컬 commit `b319e1a`. 중간 전체 검사/EXE 배포/push는 사용자 지시로 생략했다.

## 구현

- 후보17은 이어지는 원작 구역18이 있으므로 바로 종결하지 않았다. 자체9층인 **Far Amp Plains18→Amp Clearing19**를 선택했다. 원작 definition의 `(13,10,9)→(13,19,1), fixed4`, `D12P41A/m12a0801.ssb`의 banner19/ground·SSA의 player/attendant 배치·MovePositionMark를 읽어 실제 종료 ground/경로를 추출했다. 기존15→16과 각각 registry에 보존했다. 원작 메시지는 미해독이며 완료 안내는 기존 고정 한국어다.
- 원작 normal/MH 합집합13 MD를 모두 추가해 전체30 MD가 됐다. 공유 몸체5개를 재사용하고 전체 새25몸체의5,154프레임을 기존 원본 픽셀/marker/anchor/tick 검증기로 추출했다. Normal은 모두 실제 KAO와 명시 대응했다. 기존15/0 MDPE는 유지하고18의9층/타일셋15를 추가했다.
- MDER1에 기본15/새18의16층 shop/MH/trap 원래 조건·MH 출현과 shop/MH 아이템의 상대 가중치를 별도로 보존했다. 원래 floor_items와 혼합하지 않는다. 기본15는 trap density4이지만 warp6 가중치가0이다. shop/MH/warp의 불가 조건을 단순히 trap density0이라고 잘못 가정하지 않는다. 자동 맵 풀 활성화는 최종 P8 검증 때 한다.
- MDES13은 기존12 뒤에 현재 non-Oran 수량/소유자 block을 붙인다. MDES1~12를 계속 읽고 구형 획득 이력에서 사용 재고를 추정 복구하지 않는다. 열매는 기존 플레이어/동료의 실제 oran만 쓴다. 현재 수량은 물품별99개 한도이며 과거 획득 journal과 별개다. 새 non-Oran 획득부터 ledger에 한 번 등록하고 열매는 기존 자동 회복 원장으로 누적한다.
- E01에 플레이어/동료 소유별 spare Oran 예약→결과 경계 재검증→실제1개 차감/commitKey→감사 문답을 연결했다. 최소1개 회복 예비량을 남기며 부족하면 격려다. 취소는 예약만 해제한다. 일반 탐험/새 하루/복원은 실제 현재 재고를 유지한다. 거래/사건 전용 물품은 후속 P5/P6에서 연결한다.

## 간단 검증

새 event_stock, 기존 event_runtime, variety_state3개가 최종 통과했다. 새 맵9층/16문맥·normal/MH 전체 MD·별도18→19종결·소유자/현재 수량·예약 중복소비 차단·결과 전 차감 거부·나눔 once·마지막 예비량·취소/복원을 확인했다. Python NPC/대사6/6 통과.

첫 빌드의 event_registry `package.hpp` 누락을 추가했다. 검사에서 기본15의 trap density를0으로 가정한 fixture와 MDES13 뒤의 추가 block을 무시한 variety offset helper를 수정했다. 원작/포맷을 바꾸어 검사를 우회하지 않았다. SSB 조사에서 일부 미지원 opcode 경고가 있었으나 필요한 banner/ground/actor좌표가 있는 원작 스크립트는 실제 파싱했다. `filenameOf`의 시스템 파일 None과 부분 patch 문맥 불일치도 준비 과정에서 바로잡았다.

최종 EXE·원작 분포/전체 메모리·전체 구형 저장·Windows UI 검증은 P8 후 수행한다. 다음 P5는 길 안내·분실물·워프/재회의 독립 소유권/이동이다.
