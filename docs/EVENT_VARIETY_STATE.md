# 다양성 P0 — 상태·저장 계약

2026-10-21. [구현 계획](plans/2026-10-21-event-variety-implementation.md)의 P0다. 자동 이벤트·새 대사·실제 배우·휴일 방문은 아직 등록하지 않았다. 이 문서는 구현된 공통 계약과 후속 연결 지점을 설명한다.

## ID·상태

`ExplorationState.variety`는 대화1개, 주요 사건1개, 하루 일정, 독립 RNG3개, 최근 대화, 확정 결과를 보관한다. 기존 `GameplayEvent`/`DialogueStage`/`LifePhase` enum의 범위를 늘리지 않았다.

- `VarietyEvent`의1~10은 E01~E10이며 `EventIdentity`의 종류/run/day/floor/trigger가 한 사건을 식별한다. 결과 키는 `event:type:run:day:floor:trigger:result`다. 숫자는 정규 십진 표기, 문자열 ID는 영문·숫자·`.`·`_`·`-`만 허용한다.
- 화자 instance는 플레이어0, 동료 `kCompanionActorId`, 현재 population의 실제 Friend ID다. 시스템 발화만 `kSystemDialogueActorId`를 쓴다. 원본 MD/species와 혼용하지 않는다. 사건 배우는 기존 population 참조이며 player/동료/system sentinel을 NPC로 등록할 수 없다. 새 임시 배우 생성·진영은 P5/P6에서 구현한다.
- `DialogueSession`은 catalogueVersion/context/bundle/lineId/현재 문장 위치/실제 화자·상대/노출 글자 수/남은 읽기 시간/시작·기한을 저장한다. 문장·감정 catalogue 변환과 순차 출력은 P2에서 연결한다. `reconcileDialogue`는 소비자가 제공한 정확한 버전·묶음·문장 ID와 대조하며 불일치하면 종료한다. 임의의 다른 묶음으로 바꾸지 않는다.
- 사건 진행은 None→Approach/Dialogue/Action/Result/Dismiss→Finished 또는 Abandoned 범위다. P0는 이 상태의 저장·검증·중단 경계만 제공한다. 실제 사건 진행기는 P3다. 종료된 사건은 배우·예약을 소유할 수 없다.
- 예약은 현재 플레이어/동료의 실제 `oran`1개를 참조하는 의도다. 예약 기록 자체로 소비/증정하지 않는다. 이행 시 획득 이력으로 소모품을 재생성하지 않는다. 다른 물품·수량·거래 결과 적용은 P4다.

## 안전 경계와 기한

`canStartVariety(state, now)`는 탐험 평상시·양쪽 건강·동작 완료·공격 큐 없음·인접 적 없음 경계만 허용한다. 렌더에서 후보를 추첨하지 않는다. 사건/대화를 보류하는 동안 `advance`/`advanceGameplay`는 이동·전투·RNG·행동권을 진행하지 않는다. 이미 시작한 전투/피해·낮은 HP·필수 scene 전환은 사건을 취소하고 기존 진행을 계속한다.

취소는 예약·사건의 배우 참조·대화·임시 목표를 해제하고 Abandoned를 보관한다. 실제 결과 journal·재고·RNG·최근 대화·일정은 유지한다. `Explorer::invalidate`와 함께 호출하는 귀가/일반 시작/기한 만료 경로는 기존 경로 캐시도 폐기한다. 다음 탐험은 원래 탐험 목표 정책으로 재탐색한다.

대화의 안전 기한은 최대60초, 주요 사건은 최대300초다. 이는 막힘 방지 한도이며 발생 확률·연출 길이 정책이 아니다. Win32의 `nextGameplayDeadline`은 보류 중 기한을 사용해 지난 gameplay deadline을 반복 예약하지 않는다. pause/resume은 기존 시계와 함께 새 시작/기한을 이동하고 남은 읽기 시간을 유지한다. 대화/사건 중 캐릭터 교체는 보류한다.

## RNG·저장

새 실행은 `mix(seed ^ mix(runId))`에 고정 숫자 salt를 혼합해 event/dialogue/schedule 상태를 분리한다. `mix`는 기존 SplitMix64와 같은64비트 정수 혼합식이다. 구형 저장은 보관된 mapSeed/runId로 같은 규칙을 적용한다. `std::hash`와 기존 지도·출현·전투 RNG는 사용하지 않는다. `drawVariety`는 지정 stream만 진행하고 빈 후보 범위는 추첨 전에 거부한다.

MDES12는 기존 MDES11 payload 뒤에 variety block과 길이를 붙이고 전체 CRC를 유지한다. MDES1~11은 계속 읽으며 새 사건·대화·결과·최근 대화는 빈 상태다. 기존 현재 run의 meal 키는 같은 날의 식사 완료 표식으로 이행한다. MDPE1·MDCP1·원작 자산 형식은 바꾸지 않았다.

크기 한도는 대화256문장 위치/4,096글자 노출, 최근 상황64개, 확정 결과4,096개, 사건 배우kTiles개, 예약16개, 기존 전체 파일8MiB다. journal이 가득 차면 새 결과를 거부하며 이전 키를 지워 중복 지급을 허용하지 않는다. 알 수 없는 사건 enum·배우·잘못된 결과 ID·중복·길이·CRC·잘린 저장은 기존 파일 로더와 함께 거부한다. 손상된 파일을 다른 사건으로 복원했다고 보고하지 않는다.

`commitVarietyResult`는 Result 경계에서 처음인 키만 등록하고 중복이면 false다. 물건 변화나 디스크 저장을 대신하지 않는다. P4 이후 호출자는 기존 Win32 방식처럼 **candidate 복사→조건 확인·결과 변화와 키 등록→saveSnapshot 성공→실행 상태 교체→결과 문구** 순서로 연결해야 한다. 저장 전 종료는 이전 상태에서 재시도하며 저장 후 종료는 같은 키를 다시 적용하지 않는다. 취소·catalogue 변경으로 확정 결과를 되돌리지 않는다.

## 일반 실행·상세 복원·하루 인계

명시 `--explore`는 MDES12의 사건/대화/예약/배우 참조/현재 줄을 그대로 읽는다. 실제 catalogue 재생 연결은 P2, 임시 배우의 상세 복원은 P5에서 검증한다. 일반 실행은 기존 길드 아침에서 시작하며 진행 중 사건은 Abandoned로 저장하고 미소비 예약·대화를 해제한다. 같은 날의 일정/목적지·결과·RNG는 유지한다.

`Ready`에서 새 Explorer를 만드는 경로가 variety 상태를 명시 인계한다. 같은 날에는 고정한 목적지와 식사 완료를 유지하고 pending package 변경도 다음 날까지 보류한다. 다음 날이 확정되면 일정/식사 완료만 새 날짜로 초기화하고 결과·RNG·최근 대화는 유지한다. 같은 날의 서로 다른 run에서 식사 장면을 다시 보더라도 누적 식사 횟수는 한 번만 증가한다.

P0가 자동으로 지정하는 목적지는 기존 탐험뿐이다. Town/HotSpring은 저장 값만 준비했으며 휴일 선택/방문/보너스 기능을 실행하지 않는다. 빈도·교환 비율·보상·휴일 주기는 여전히 미정이다.

## 재현

```sh
cmake -S . -B build/p0 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/p0 --target pet_variety_state_tests pet_package_transition_tests
ctest --test-dir build/p0 --output-on-failure -R '^pet_(variety_state|package_transition)_tests$'
```

`pet_variety_state_tests`는 독립 RNG 고정 벡터·실제 탐험 비간섭, 동일 frame 반복 호출·pause·기한, catalogue 불일치, 저장 전후 결과 재시도·실패 저장의 이전 파일 보존, 같은 날/다음 날 인계·식사 once·pending package 보류, 손상·MDES9~11 이행을 검사한다. MDES1~8·원작 패키지·전투 회귀는 기존 검사에 유지하며 legacy fixture 생성기는 새 block을 실제 제거한 뒤 구형 payload를 만든다.
