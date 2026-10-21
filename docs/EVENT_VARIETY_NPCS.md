# 다양성 P1 — 실제 NPC 몸체·얼굴 연결

2026-10-21. [구현 계획](plans/2026-10-21-event-variety-implementation.md)의 P1이다. 기본 탐험의 논리 MD를 실제 몸체·초상화에 연결하며, 방문 배우의 자산을 준비한다. 새 대사/자동 방문/사건 진행기는 P2/P3 범위다.

## 선택 범위와 원작 대응

| 범위 | MD | 용도 |
| --- | --- | --- |
| 기본 던전15 전체 normal 출현 |83,126,193,237,240,341,342,437 |7개 층의 원래 상대 가중치를 유지하며 표시 |
| 구형 던전0 전체 normal 출현 |16,102,191,293,425 |구형 저장·명시 실행 호환 |
| 길드 방문 준비 |369,434 |가재군 actor115/MD369, 비버니 actor124/MD434의 Walk 포함 |
| 기존 명시 배우 호환 |383,384 |과거 테스트/저장 속 실제 켈리몬. 관계에 따른 대체 그림으로 사용하지 않음 |

총17 MD/17개 몸체 참조다. 스타팅의 MD425 몸체와 기존 켈리몬383/384를 재사용하고, 나머지14개 몸체를 기존 `prepare_party_actor`로 생성했다. 원작 WAN→병합→선택 atlas의2,730프레임은 픽셀·marker/anchor/그림자·tick을 대조했다. Walk0/Attack1/Hurt6/Idle7은 원작8방향을 요구한다. 결손 Nod22/Faint33을 다른 동작으로 복제하지 않으며 기존 유한 Idle/Hurt 응답을 유지한다.

`npcs/selection.json`은 MD→명시 KAO행/한국어 표시 이름/선택적 방문 actor ID의 편집 원본이다. `prepare_npc_characters.py`가 원본 monster.md·tbl_talk·KAO와 기존 몸체 출처를 대조하고 `characters.mdnc`/감사 JSON/몸체/공유 초상화를 만든다. 한국어 이름은 제품 표시 이름이며 패치 ROM 문자열을 해독했다고 주장하지 않는다. 예를 들어 MD237은 species210·sprite257·KAO236이다. 이름·도감 번호·원본 MD·배우 instance·KAO행을 혼용하지 않는다.

## 런타임 계약

MDNC1은 MDES12/MDPE1과 별도 형식이다. ROM hash와17개의 MD/species/sprite/KAO/감정 bit mask/원작 talk group/방문 actor ID, 몸체 key·PNG hash·상대 경로·캐릭터 identity·표시 이름을 저장한다. 경로 탈출·중복 MD/방문 ID·범위 밖 값·잘림·미지원 버전을 거부한다. 원작 talk group을 새 한국어 성격으로 해석하지 않는다.

`NpcCatalogue::forActor`는 현재 population의 Friend instance를 찾아 originalId로 조회한다. 플레이어0·동료 sentinel·아이템·없는 instance·미등록 MD는 거부한다. 우호/적대 목록은 몸체 선택에 관여하지 않는다. 출현표의 양수 후보가 전부 등록됐는지 기본/구형 맵 모두 검사한다. 다른 종 그림으로 대체하거나 출현 가중치를 재정규화하지 않는다.

Win32는 현재 층 population에 필요한 몸체만 경로별 공유 캐시에 넣는다. 같은 몸체를 쓰는 두 instance가 있어도 PNG는 한 번 decode한다. 다음 상태의 새 자산은 먼저 로드·검증하고, 기존 snapshot 저장 성공 후 교체한다. 캐시에서 제거할 Package의 frame을 가리키는 enemy/NPC Animator를 먼저 정지·해제한다. 캐시가 같으면 매 frame PNG를 다시 읽지 않는다. 길드 방문은 자산/원본 actor 대응만 준비했으며 자동 등장은 P3다.

실제 NPC가 말할 때 Friendly 역할의 고정 켈리몬 cast 대신 그 instance의 `DialogueActor`를 compositor에 전달한다. 플레이어/동료/알람은 기존 선택 캐릭터 cast를 유지한다. 초상화 하나·명찰 없음·기존200px 화면/최종 fade 규칙을 유지한다.

`originalEmotionMask`는 원작 KAO의 보유 감정이고 실행 `emotionMask`는 검증된 보완까지 포함한다. 켈리몬383의 기존 보완 감정11개는 출처/라이선스와 함께 유지한다. 새 NPC는 보유 원작 감정 또는 같은 배우 Normal을 쓴다. Normal이 없는 배우의 발화 역할은 `npcEmotion`에서 거부하며, 적 몸체 지원과 구분한다. 이번17 MD는 모두 Normal을 검증했다. trace에는 요청 감정과 실제 감정, 실제 화자 MD를 함께 남긴다.

## 재생 호환성 회귀

실제 불꽃숭이 NPC의 대화 종료 후 이동에서 `direction phase incompatible`가 발견됐다. 동작 캐시에 Idle/Nod가 남거나 방향별 주기가 다른 경우 Walk 방향 변경으로 처리할 수 없었다.

`Animator::tryChangeDirection`은 **같은 clip의 행이며 loop·주기가 호환될 때만** 재생 위치를 유지한다. 호환되지 않으면 수정 없이 false를 반환하며 NPC 동기화 쪽에서 올바른 Walk/Idle을 시작한다. 원본 tick·행·그림을 바꿔 주기를 억지로 맞추지 않는다. 기존 `changeDirection`의 엄격한 실패 계약은 유지한다. alias가 자체 행을 보유한 경우도 실제 재생 행을 검사한다.

## 재현·패키징

```sh
python -m tools.prepare_npc_characters --out build/npc-selected
cmake -S . -B build/p1 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/p1 --target pet_npc_character_tests pet_character_scene_tests pet_dialogue_portrait_tests
ctest --test-dir build/p1 --output-on-failure -R '^pet_(npc_character|character_scene|dialogue_portrait)_tests$'
python -m unittest tests.test_npc_assets tests.test_character_assets tests.test_dialogue_portraits -v
```

생성물의 `npcs/`와 `portraits/`를 gameplay에 반영한 뒤 기존 CMake의 gameplay 복사/설치 경로와 `check_windows_runtime.py`가 전체 선택 자산을 포함한다. 런타임 JSON 파서나 새 상주 의존성은 추가하지 않았다. 같은 입력에서 새 실행 catalogue/몸체/얼굴 바이트가 재현되고 감사 JSON 의미도 같다. ROM·출현/층 MDPE·스타팅 몸체는 변경하지 않았다.

`pet_npc_character_tests`는17개 전수의 필수 방향·정체성·우호/적대 비간섭·복원·Normal 대체·시스템 sentinel 배제·잘못된 바인딩 거부·새 실제 NPC overlay를 검사한다. Python은 원본 MD/KAO/tbl_talk와 실제 픽셀을 독립 비교한다. 기존21종/역할 반전/성별·나옹·먹고자 검사를 재사용한다. 실제 EXE 검증과 원격 상태는 [P1 로그](logs/2026-10-21-event-variety-p1.md)를 따른다.

기존 Windows 회귀용 export는 합성 MD 대신 명시 원작383/384를 저장한다. 순수 core fixture와 분리하며 `pet_npc_character_tests --validate-fixtures <exploration.mdpe> <snapshot-directory>`로 내보낸 모든 NPC의 등록·몸체·얼굴을 검사할 수 있다. 제출 픽셀 검사도 trace의 실제 MD 바인딩을 조회한다.
