# MDES3 저장 형식

정의: `src/core/state_store.cpp`의 encodeSnapshot/decodeSnapshot. 모든 수는 little-endian. enum·bool·타일/count는 u32, 시간·RNG·run/round는 u64, 문자열은 UTF-8 byte 길이(u32)+내용이다. 맨 끝에 이전 모든 bytes의 IEEE CRC32(u32)를 둔다. 최대 파일 크기·count·참조·상태를 읽기/쓰기 양쪽에서 검사한다.

## 기본 부분과 확장 순서

1. MDES magic / schema3, assetVersion·runId·mapSeed·RNG, 원작 floor/profile·배치 정책.
2. 현재 층·World·Population 기본 tuple(instance/original ID, kind, tile, done), omission, 플레이어 위치·visited/blocked·상한/phase.
3. inventory·onceKeys, target(+1 인코딩)·goalKind. 여기까지 MDES1의 기본 형식이다.
4. enabled/defeated/homeScene, LifePhase/CombatPhase, HP·열매·피해·식사·날짜·명중, phaseStartedMs/deadlineMs, 적대 목록·gameplay onceKeys. 여기까지 MDES2와 동일한 순서다.
5. discovered/failedTargets 각576 u32, visitedRooms/stalledActions/targetFailures/knownStairs(+1)/targetInstance.
6. actionRound/playerActionRound/playerDirection, playerHidden/fadeInOnly, DeathStage/SceneBeat/DialogueStage.
7. Population 순서로 각 배우 Tracking/lastSeen(+1)/chaseTarget(+1)/blockedTurns/direction/remainingHits/actionRound/visible.
8. walkStartedMs, 플레이어 MoveRecord, Population 순서의 각 MoveRecord, CRC32.

MoveRecord는 actionId/startedMs(u64), durationMs/direction/from(+1)/to(+1)(u32) 순서다. actorId는 소유 슬롯(플레이어0 또는 entity.instanceId)으로 정해진다. actionId0은 빈 구간이며, 유효 이동은 duration400ms·공통 코너 판정·논리 도착 타일 일치를 검사한다. 실시간 표시는 단조 증가 시계로 진행하고 UTC 저장값은 재실행 시 남은 구간과 보행 위상 복원에 사용한다.

기존 enum 번호는 유지하고 Death·Dialogue·PlayerFaint 등은 끝에 추가했다. SceneBeat는 이번 버전의 생활 세부 단계다. 정의되지 않은 schema/enum, 잘린 파일, 잘못된 CRC·경계·참조·상태를 거부한다.

## 이전 저장 읽기

MDES1·2를 읽어 누락 필드를 안전하게 채운 뒤 다음 저장부터 MDES3으로 쓴다. runId·RNG·플레이어 위치·현재 HP·완료 기록·inventory는 보존한다. 발견 정보는 visited에서 시작하고, 과거의 점유 타일 목표는 재계획한다. 진행 중 전투는 기존 phase로부터 행동 소비와 적 잔여 적중수를 복원한다.

과거 완료 NPC는 원래 제거된 상태를 유지한다(visible=false). 미처리 접촉으로 플레이어와 겹친 과거 NPC는 가장 가까운 비점유 바닥으로 재배치하며 대화 완료나 새 onceKey를 만들지 않는다. 공간이 없으면 비표시로 두고 기존 안전 귀가를 허용한다. 새 버전의 완료 NPC는 visible=true와 done=true를 함께 저장해 표시·점유를 유지한다.

## 중복 방지와 중단

논리 상태를 durable commit한 뒤 이동/문구/연출을 표시한다. 임시 파일·flush·원자적 교체는 기존 방식을 유지했다. 재시작으로 공격·아이템 소비·식사·대화 완료·귀가·다음날 초기화를 다시 적용하지 않는다.

일시 중단은 이동과 애니메이션·장면 타이머를 함께 보정한다. 기존 EventSleep의 UTC 마감은 계속 경과한다. 렌더 완료는 플레이어·적·동료 신호로 분리하고, Faint는 플레이어·Wake/Nod는 두 배우 완료를 기다린다.
