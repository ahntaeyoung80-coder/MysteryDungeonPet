# Adaptive Desktop Pet — 원작 데이터·애니메이션·장면 매핑

작성 기준: 2026-10-13 KST. 이 매핑은 Phase 1~3의 확인된 경로·정규화 규칙을 결합한다. 실제 ROM을 추출한 manifest가 아니라 후속 추출기와 runtime의 데이터 계약이다. 알 수 없는 원본 인덱스는 null/unresolved로 둔다.

최신 실행 범위: `assets/original/gameplay/provenance.json`이 MD1/MD4 전투·생활 clip 및 G01P06B/G01P07A 집 map의 실제 선택 추출·crop/hash를 기록한다. 원본 ROM 전체/종별 지원 판정과 구분한다. 이번 최소 전투에서는 m_attack의 Attack/Hurt/Faint를 사용하며 아래 과거의 전투 제외 문구를 덮어쓴다. 오랭열매70 사용은 text-only이며 별도 clip을 연결하지 않는다.

## 1. 서로 다른 ID 체계

| ID 공간 | 원본 위치 | 의미·정규화 키 | 금지할 연결 |
| --- | --- | --- | --- |
| ROM edition | ROM.idCode+arm9 식별+Pmd2Data | 모든 ROM 키의 namespace,hash/언어 포함 | NA 주소/StringBlocks를 EU/JP에 고정 |
| dungeonId | ARM9 정의/특례 | edition:dungeonId | 이름 배열 길이=플레이 가능 던전 수 가정 |
| localFloor / groupFloor | DungeonDefinition+mappa | localFloor0-based; groupFloor=start_after+localFloor | GUI 그룹 floor_id에 start_after를 다시 추가 |
| mappaIndex | DungeonDefinition | floor_lists의 그룹 index | dungeonId와 동일 값 가정 |
| mdIndex | BALANCE/monster.md·층 monsters | 원본 개체/폼/성별 행 | 국가 도감 번호·SpriteCollab directory 번호와 동일 가정 |
| spriteIndex | MdEntryProtocol의 signed field | MONSTER pack의 공통 sprite 참조 | mdIndex를 sprite entry로 바로 조회 |
| nationalDex/baseForm/gender | MD 레코드 | 외부 매핑 근거,값별 원본 보존 | mdIndex%num_entities를 외부 form index로 사용 |
| collabPath | tracker.json subgroup | species/form/color/gender 경로 문자열 | ROM form 번호와 하위 폴더 번호를 수식으로 자동 일치 |
| WAN animationId | WAN sequence/AnimData.xml Index | 각 asset 내부 동작 번호 | SpriteBot actions 배열 순번 사용 |
| scriptAnimationCode | SSB SetAnimation Number | SETANIMATION_TABLE+flags로 WAN 변환 | code64를 WAN64로 재생 |
| direction SSA/SSB/atlas | SSA1..8 / SSB0..7 / sheet rows | SSA-1, SSB 그대로 → row0..7 | SSA2와 SSB2를 같은 방향 취급 |
| actorId/objectId | SSA 및 script_data 설정 | 배치의 식별자 | 배열 index/MD index와 동일 가정 |
| scriptLevelId/mapId | back_SetGround / script level | level→mapid→bg_list | level210을 bg_list210으로 직접 조회 |
| effectId/graphic slot | SetEffect / 효과 설정·pack | raw ID+resolved reference | effect642=pack entry642로 가정 |
| itemIndex/itemId/categoryId | item_p/mappa/config | 원본 record index,데이터 ID,category를 분리 | guaranteed65535를 랜덤 가중치로 사용 |
| item instanceId | 새 world/inventory | 층 seed+생성 ordinal 등의 stable key | 원본 itemId만으로 개별 획득을 식별 |

## 2. ROM 자산 연결

| 원본 데이터/파일 | 파서·원본 field | 변환 산출물(제안) | runtime 연결 |
| --- | --- | --- | --- |
| ARM9 DUNGEON_DATA_LIST | HardcodedDungeons.get_dungeon_list; floors/group/start_after | dungeons catalogue | Explorer dungeon/localFloor → FloorRecord |
| BALANCE/mappa_s.bin | MappaBinProtocol.floor_lists,layout,monsters,item lists | FloorRecord + raw/relative distributions | WorldGenerator에 tileset ID,SpawnSampler에 normal/items |
| MESSAGE/text_*.str | StrHandler.strings+StringBlocks | language strings/name references | 표시 이름; ID 대신 이름으로 저장하지 않음 |
| BALANCE/monster.md | mdIndex,entid,national_pokedex_number,base_form_index,gender,sprite_index | PokemonRecord+ROM assetKey | spawn mdIndex → 원본 spriteIndex → Animator |
| BALANCE/item_p.bin | item_id,category,sprite,palette,is_valid | ItemRecord+iconKey+foodKind | 수집 icon/생활 음식 분류 |
| MONSTER/monster.bin | BIN_PACK spriteIndex,PKDPX WAN | original dungeon motions | Walk/Idle/Pose 등; ground/attack과 통합 |
| MONSTER/m_ground.bin | 같은 index의 raw WAN | 생활 동작 | Eat/Sleep/EventSleep/Wake/Nod 등 실제 지원된 항목 |
| MONSTER/m_attack.bin | 같은 index의 PKDPX WAN | 통합 reference,선택 동작만 | 전투 제거,필요한 시각 clip만 유지 |
| DUNGEON/dungeon.bin | logical files는 pmd2dungeondata.xml로 복원 | tileset/item atlas+DMA rule/palette ticks | 새 terrain grid→원작 chunk; item sprite/palette |
| MAP_BG/bg_list.dat+*.bma/bpc/bpl/bpa | BgListEntryProtocol+MapBgModule/BMA | selected background/layer frames | 생활 장면 backgroundKey |
| GROUND/{object-name}.wan | script_data.objects에서 name 연결 | object sprite+anchor/tick | 식사/수면 소품 |
| FONT/kaomado.kao | portrait archive | 선택 portrait | 대화 UI가 필요할 경우만; 몸체 그림으로 쓰지 않음 |
| SCRIPT/*/*.ssb + SSA/SSE/SSS | Ssb/Ssa parser+SourceMap | event cue+source provenance | EventPlayer; 원작 VM 전체는 미도입 |

정확한 코드 링크는 [01_DATA_EXTRACTION.md](01_DATA_EXTRACTION.md), [02_ANIMATION_ASSETS.md](02_ANIMATION_ASSETS.md), [03_EVENT_BEHAVIOR.md](03_EVENT_BEHAVIOR.md)의 파일별 표에 있다. 원본 sprite pack 해제 흐름은 [skytemple/module/sprite/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/sprite/module.py), pack 논리 이름은 [skytemple_files/_resources/ppmdu_config/pmd2dungeondata.xml](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/_resources/ppmdu_config/pmd2dungeondata.xml)을 기준으로 한다.

## 3. 분포와 지형 규칙

MonsterSpawn의 normal/MH,ItemSpawn의 floor/shop/MH/buried/unknown은 서로 다른 context이다. rawCDF는 그대로 보존하고 relativeWeight는 직전 nonzero 누적값과의 차분으로 만든다. 아이템은 category마다 CDF를 분리하고 guaranteed를 제외한 뒤 차분한다. derived probability는 P(category)×P(item|category)이며 배치 개수·시간당 출현율과 별도다.

FloorRecord는 원본 layout을 provenance용으로 남기고 runtime geometry는 새 2~3방 generator로 대체한다. 원본 층의 tileset과 출현 종·아이템 상대 분포는 유지한다. 신규 spawn count/생활 주기는 original 필드로 표기하지 않는다.

원작 던전 tileset t0..169의 pack 슬롯은 DPLA=t,DMA=170+t,DPC=340+t,DPCI=510+t,DPL=680+t이다. 배경군 b0..28은 DPLA850+b,DPC879+b,DPCI908+b,DBG937+b,DPL966+b이다. 이 슬롯은 문서 조사 스냅샷의 설정에 근거하며 실제 ROM 범위 검사를 통과해야 유효하다.

DMA 지형 WALL0/WATER1/FLOOR2와 이웃 비트 S01/SE02/E04/NE08/N10/NW20/W40/SW80을 적용한다. Phase 4 실험의 byte0/1은 단순 wall/floor 여부이며 DMA enum과 같은 값이 아니다. 실험 floor1을 runtime DMA FLOOR2로 명시 변환한다. 물·장식은 walkability와 별도 layer로 표현한다.

계단은 새 generator가 좌표를 만들고 original graphic reference를 붙인다. DmaExtraType을 계단 그림으로 사용하지 않는다. 원본 계단 graphic slot/frame은 unresolved이므로 확정 자산 목록에 가짜 값으로 넣지 않는다.

## 4. 동작·이벤트 ID 매핑

| 생활/탐험 행동 | WAN Index | 확인된 script code | 재생·종별 지원 정책 |
| --- | --- | --- | --- |
| 걷기 | 0 | Idle 특수 경로와 일반 테이블을 구분 | 8방향 Walk,이동 속도는 Explorer |
| 기본공격 | 1 | 전투 경량 상태가 직접 선택 | MD1/MD4의 원작 Attack 유한 재생; 공격355만 사용 |
| 피격 | 6 | 전투 경량 상태가 직접 선택 | MD1/MD4의 Hurt 유한 재생; 패배 감정 cue에도 신규 재사용 |
| 적 기절 | 33 | 전투 경량 상태가 직접 선택 | 원작 Faint 완료 뒤 승리; 같은 층·위치 재개 |
| 바닥 충돌 | 34 | 현재 별도 선택 없음 | 원작 HitGround 지원 보존; 필요할 때만 후속 연결 |
| Idle | 7 | 1/2 특수 idle 선택 가능 | 원본 loop/freeze 규칙과 종별 지원 |
| Run | 별도 확인 못 함 | 미확인 | Walk 이동 속도 증가를 declared_fallback으로 표시 |
| Sleep | 5 | 6→0x0805 | 생활 deadline까지 루프 |
| EventSleep | 13 | 22/42/62→0x100D/080D/080D | 방향별 수면; 종별 지원 검사 |
| Wake | 14 | 23/43/63→0x100E/080E/100E | finite 종료+대기 후 Idle |
| Eat | 15 | 24/44/64→0x100F/080F/080F | 소품/효과+once 소비; 모든 NPC의 Eat 동작이라고 가정하지 않음 |
| Pose | 17 | 26/46/66→0x1011/0811/1011 | 마지막층 클리어 시 유한 포즈 |
| Nod | 22 | 31/51/71→0x1016/0816/1016 | 인사 후보,교류 cue는 신규 |
| Hop | 10 | table 실제 항목 확인 후 선택 | 기쁨 후보,지원 clip만 사용 |
| Laying | 27 | 76→0x081B | 수면 장면의 누운 자세 |
| 놀람 | 미확정 | 미확정 | Shock4의 의미·픽셀 검증 없이 생활 놀람으로 채택하지 않음 |

AnimationRecord에는 원본Index와 script code를 별도 저장한다. CopyOf는 aliasOf로 유지하고 없어진 PNG를 missing으로 오판하지 않는다. loop/speed/flag/rawWord를 잃지 않게 cue에 보존한다. tickHz=60은 미확정 초기 가정이며 SpriteBot GIF의 duration×20ms를 원작 타이밍으로 쓰지 않는다.

## 5. frame/anchor 정규화 예시

실제로 확인한 공개 Pikachu Walk 샘플을 설명하는 JSON이다. 원본 ROM mdIndex/spriteIndex는 추출하지 않았으므로 null이다. 최종 packed atlas의 좌표가 아니라 공개 원본 시트의 first-frame 좌표다.

~~~json
{
  "schemaVersion": 1,
  "assetKey": "public:SpriteCollab:0025:base",
  "romIdentity": {"edition": null, "mdIndex": null, "spriteIndex": null},
  "sourceCommit": "e4b1949687cb2344e6675717466e357189d3acba",
  "sourceDirectory": "sprite/0025",
  "animation": {
    "name": "Walk", "wanIndex": 0, "rows": 8,
    "frameSize": [32, 40], "durationTicks": [8, 10, 8, 10],
    "sourceRectFirstFrame": [0, 0, 32, 40],
    "sheetReferenceCenter": [16, 24],
    "tickHz": null, "markerVerification": "pending",
    "status": "exact_public", "romPixelMatch": "unverified"
  }
}
~~~

runtime row 순서: Down,DownRight,Right,UpRight,Up,UpLeft,Left,DownLeft. Sleep/Eat 같은 1행 clip은 directional=false로 기록한다. cell anchor=(w//2,h//2+4)를 기본으로 하되 Offset marker의 실제 좌표·WAN offset를 합쳐 원본 기준점을 보존한다. cropOrigin이 (cx,cy)이면 frame rectangle과 anchor/head/lhand/rhand/center/shadow 모두 같은 원점에서 빼야 한다.

Straight RGBA PNG atlas를 WIC가 PBGRA로 디코드하면 renderer는 premultiply를 반복하지 않는다. 원래 팔레트 투명 인덱스→alpha 변환은 오프라인 단계에서 보존한다. 이미지 검정색을 일괄 제거하지 않는다.

## 6. 실제 장면과 리소스 연결

| 장면 | 원본 근거 / 명령 | 배경·소품 | 정확성·누락 |
| --- | --- | --- | --- |
| 식사 | G01P06B/c00a0601.ssb;SetAnimation64(WAN15),effect642/643 | scriptLevel210→mapid/bg_list→g01p06b;object107→GROUND/g01p06b1.wan | SSA actorId0=(220,184),10=(196,184),Down. object=(256,212). 효과 slot 미확인 |
| 음식 관련 NPC | c00a0601/n01a0601;actor79 effect624AppleOnHead,Wait20 계열 | 같은 원본 식사 공간 | NPC별 clip은43/42 등 다름. 효과624의 실제 graphic 연결 미확인 |
| 공통 수면 | COMMON/unionall routine22:6,Wait240 반복 | 선택 생활 공간 | Sleep5loop 확인. 시스템 수면 시간은 신규 |
| 방향별 수면 | unionall routine48/49:62,SSB dir2,Wait60 | 장면에 맞는 배경 | SSB2는 Right,Return/Hold 종료 차이 |
| 기상 | unionall routine26/27:62→Wait60→63→WaitAnimation→Wait10→2 | 선택 생활 공간 | 방향 전환과 추가Wait40 포함. loop·완료 시점 원작 wall time 검증 대기 |
| 누운 기상 | G01P07A/c00a0201.ssb:level211,76→63 | MAP_BG/g01p07a.*,GROUND/g01p07a1.wan | 원본 object 역할·그림 내용 최종 QA 필요 |
| 수면 공간 | G01P07C/m02a0113.ssb:level212,76,62,Wait90/120 | MAP_BG/g01p07c.* | 전체 story cue는 제외 |
| 인사/기쁨 | Nod22/Hop10+방향 | 친구 몸체 clip | 구체적인 원작 인사 SSB 미고정;신규 cue로 명시 |
| 발견/획득 | overlay29 pickup API 선언+원본 item icon | dungeon item atlas | 특정 원작 발견 효과/clip 전체 미확정 |
| 클리어 | WAN17/Pose와script26/46/66 | 선택 원본 클리어 자산 | 원작 Pose17 사용 확정;부가 카메라/효과 상세는 별도 |

S01P02A/conquest.ssb/return.ssb는 귀환 문맥 자료이며 Pose 호출의 근거가 아니다. 원작 Pose17을 사용하는 개발 기준은 확정되어 있다. 원작 tick/효과 재현 검증과 이 기준을 혼동하지 않는다.

### 화면·배치 계약 보완

Renderer 입력에는 cameraOriginScenePx, viewportSizePx, integerScale을 명시한다. 논리 tile→장면 pixel→DIB 변환과 Windows 창 위치는 별개다. frame crop/anchor는 같은 장면 원점으로 합성하고 경계 clipping은 [05_WINDOWS_RUNTIME.md](05_WINDOWS_RUNTIME.md)를 따른다.

SpawnSampler는 가용 타일에서 비복원 추출하며 spawnBudget 및 가용 수를 넘지 않는다. guaranteed 원본 목록은 보존하고 초과 요청은 미배치 사유로 남긴다. 상세 종료·검사 기준은 [04_DUNGEON_AI.md](04_DUNGEON_AI.md)를 따른다.

## 7. 패키지와 지원 감사

제안 manifest의 assetSources[]에는 sourceKind(rom/collab),원본해시/커밋,sourcePath/packSlot,md/sprite 참조,license/credit,변환도구커밋,contentHash,status,fallbackReason을 남긴다. runtime은 안정적인 assetKey만 사용하고 원본 ID 추적은 catalogue가 맡는다.

선택 단위는 PokemonForm+필수동작,tileset,food/item icon,background/effect이다. alias/공통 프레임·팔레트는 hash로 deduplicate한다. 디버그 Offsets/Shadow 시트 대신 정규화 marker manifest를 넣을 수 있다. 독립 지연 로드 캐시도 설치물/다운로드 데이터 용량에 포함해 보고한다.

전 종/폼의 필수 동작 지원은 tracker/XML 감사 뒤 필요한 PNG를 검사하는 순서로 진행한다. exact_rom/exact_public/alias_exact/declared_fallback/missing/malformed/original_mismatch를 나눠 support report를 만든다. 지금은 두 종 XML과 Pikachu8PNG의 구조·전체 디코드 검사까지이며 원작 전체 매핑은 생성되지 않았다.

미확인 asset key는 설치 manifest에 필수 리소스로 확정하지 않는다. effect642/643/624,계단 그림,원작 클리어 효과는 검증 gate에 남긴다. source 정책 확인 없이 공개/원작 이미지를 재배포 허용으로 표시하지 않는다. 이 조사 ZIP에는 보고서 텍스트만 포함한다.
