# Phase 2 — 스프라이트 및 애니메이션

작성 기준: 2026-10-13 KST. Phase 1의 ROM 원본 ID와 sprite_index를 재사용한다. 원작 보존의 기본 자산은 사용자가 제공한 ROM의 WAN이다. SpriteCollab은 공개 시트 포맷·검사·누락 보완 후보로 사용하며 공개 시트가 모든 원작 그림과 동일하다고 가정하지 않는다.

## 1. 실제 소스와 재사용 결정

| 용도 | 파일·함수 | 입력 → 출력 | 적용·의존성·라이선스·확인 수준 |
| --- | --- | --- | --- |
| ROM WAN 파싱 | [skytemple_files/graphics/chara_wan/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/chara_wan/handler.py) / CharaWanHandler.deserialize, FileType.WAN.CHARA | SIR0 WAN bytes → WanFile | 오프라인 직접 사용; Python/Pillow/설정; GPL-3.0-or-later; 소스 확인, ROM 미실행 |
| 3개 WAN 통합 | 같은 handler.py / merge_wan(wan_monster,wan_ground,wan_attack) | 3 WanFile → 통합 WanFile | 식사·수면·기상 같은 ground 동작을 누락하지 않도록 통합; 본문에서 MergeWan 호출 |
| 시트 출력 | 같은 파일 / export_sheets(out_dir,wan,sprite_def), export_sheets_as_zip | 통합 WAN, Pmd2Sprite → XML+PNG | 기본 변환기로 채택; 종별 anim name map 필요; 오프라인 비용만 발생 |
| 시트 검증·역변환 | 같은 파일 / import_sheets(in_dir,strict=False), split_wan | XML/PNG → WanFile / WAN 목록 | strict 검사 및 왕복 검사 후보. 양자화 경로가 있어 원본 atlas 제작에 재-import를 강제하지 않음 |
| 시트 구현 | [skytemple_files/graphics/chara_wan/sheets.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/chara_wan/sheets.py) / ImportSheets, ExportSheets, ImportSheetsFromZip, ExportSheetsAsZip | 디렉터리·그림·anim_name_map → WAN 또는 파일 | 실제 함수이며 클래스가 아님; CopyOf·색상 마커·프레임 bounds 로직 참고 |
| 통합·축소·분리 | [skytemple_files/graphics/chara_wan/split_merge.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/chara_wan/split_merge.py) / MergeWan,ReduceWan,SplitWan | WAN 객체 → 변환 WAN | 필요한 동작만 남기는 오프라인 후보. 원본 frame/duration/anchor 변화 검사 필수 |
| 공개 시트 검사 | [SpriteUtils.py](https://github.com/PMDCollab/SpriteBot/blob/8a502bf4553b816d6d3bc2c0d5f2bfd8550690ef/SpriteUtils.py) / getStatsFromTree,verifySprite,getFramesAndMappings | XML/PNG/zip → 검사·프레임 정보 | SpriteBot 전체 실행 대신 함수 흐름 참고. GPL-3.0 코드 이식 시 해당 조건 검토 |
| 폼 목록 순회 | [TrackerUtils.py](https://github.com/PMDCollab/SpriteBot/blob/8a502bf4553b816d6d3bc2c0d5f2bfd8550690ef/TrackerUtils.py) / TrackerNode,iterateTracker,getNodeFromIdx,getDirFromIdx,getCurrentCompletion | tracker 트리/인덱스 → 경로·상태 | 자동 목록화에 사용. tracker 완료 표시와 PNG/동작 존재 검증을 분리 |

SkyTemple ROM 접근 기준은 [skytemple/module/sprite/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/sprite/module.py)의 get_monster_monster_sprite_chara, get_monster_ground_sprite_chara, get_monster_attack_sprite_chara이다.

| ROM 파일 | BIN_PACK 하위 데이터 | 호출 흐름 |
| --- | --- | --- |
| MONSTER/monster.bin | PKDPX 압축 WAN | sprite_index → BIN_PACK entry → PKDPX.deserialize(...).decompress() → WAN.CHARA.deserialize |
| MONSTER/m_ground.bin | 압축되지 않은 WAN | 같은 sprite_index → BIN_PACK entry → WAN.CHARA.deserialize |
| MONSTER/m_attack.bin | PKDPX 압축 WAN | monster.bin과 동일한 해제 흐름 |

3개 pack의 인덱스 범위를 각각 확인한다. 누락·잘못된 sprite_index를 0번 그림으로 조용히 대체하지 않는다. MD 이름·성별·폼과 원본 WAN 통합 결과를 manifest에서 연결한다.

## 2. SpriteCollab 경로와 AnimData.xml

고정 커밋 e4b1949687cb2344e6675717466e357189d3acba. 실제 샘플: [sprite/0001/AnimData.xml](https://github.com/PMDCollab/SpriteCollab/blob/e4b1949687cb2344e6675717466e357189d3acba/sprite/0001/AnimData.xml), [sprite/0025/AnimData.xml](https://github.com/PMDCollab/SpriteCollab/blob/e4b1949687cb2344e6675717466e357189d3acba/sprite/0025/AnimData.xml). 그림은 같은 디렉터리의 Walk-Anim.png, Walk-Offsets.png, Walk-Shadow.png처럼 이름별 3개 시트이며 credits.txt가 있다. 종 아래 폼·색상·성별 하위 경로는 [tracker.json](https://github.com/PMDCollab/SpriteCollab/blob/e4b1949687cb2344e6675717466e357189d3acba/tracker.json)의 subgroup 트리에서 순회한다. 모든 종이 sprite/{번호}/만으로 끝난다고 가정하지 않는다.

| XML 노드 | 의미·규칙 |
| --- | --- |
| AnimData/ShadowSize | 그림자 크기 선택값; 샘플 계열 0..2 |
| AnimData/Anims/Anim/Name | Walk/Eat 등의 식별 이름 |
| Index | WAN 애니메이션 번호. 일부 항목에서 생략될 수 있어 엄격 검사·개별 처리 |
| CopyOf | 다른 동작 이름의 시트 재사용. 별도 PNG가 없을 수 있음; 순환·없는 대상 검사 |
| FrameWidth,FrameHeight | 시트를 나눌 고정 셀 크기 |
| Durations/Duration | 시간축 열마다 지속 tick; 순서 보존 |
| RushFrame,HitFrame,ReturnFrame | 해당 프레임 마커; 시점 정보로 저장, 전투 로직은 도입하지 않음 |

가로 열은 시간, 세로 행은 방향이다. 기본 방향 순서는 Down, DownRight, Right, UpRight, Up, UpLeft, Left, DownLeft이다. 일부 동작은 한 방향 행만 있다. 1행 동작은 그 사실을 기록하고 8행처럼 존재하지 않는 방향을 읽지 않는다.

SSA 방향 1..8은 시트 row=ssaId-1, SSB 방향 0..7은 row=ssbId이다. 실제 기준은 [skytemple_files/common/ppmdu_config/script_data.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/common/ppmdu_config/script_data.py)의 Pmd2ScriptDirection.ssb_id이다. 원본 방향 값과 정규화 row를 함께 저장한다.

공개 설정 sprite_config의 actions 배열 순번을 원작 XML Index나 스크립트 SetAnimation 번호로 쓰지 않는다. 이 세 ID 체계는 서로 다르다. 스크립트 변환은 Phase 3의 SETANIMATION_TABLE을 사용한다.

## 3. 동작 ID와 대체 정책

[skytemple_files/_resources/ppmdu_config/pmd2spritedata.xml](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/_resources/ppmdu_config/pmd2spritedata.xml)은 종별 스프라이트 정의와 애니메이션 이름을 제공한다. MONSTER/sprconf.json이 있으면 Phase 1의 ROM 설정 보완을 적용한다. 다음은 확인된 기본 계열이며 모든 포켓몬의 모든 WAN에서 지원한다는 보장은 아니다.

| 필요한 행동 | 기본 WAN Index | 재사용·누락 처리 |
| --- | --- | --- |
| Walk | 0 | 원본 방향별 보행을 기본 이동으로 사용 |
| Idle | 7 | 자체 시트·CopyOf·원본 idle 변형을 구분 |
| Run | 미확인 | 조사한 2개 XML과 기본 테이블에서 별도 Run 확인 못 함. Walk의 이동 속도 변경을 declared_fallback으로 명시 |
| Sleep | 5 | 한 방향 수면과 이벤트 수면을 구분 |
| EventSleep | 13 | 생활 장면의 방향별 수면 후보 |
| Wake | 14 | 유한 기상 동작; 끝난 뒤 Idle |
| Eat | 15 | 음식 소비 시점은 신규 이벤트 cue로 지정; 원본 반복/종별 차이 반영 |
| Pose | 17 | 던전 클리어 포즈로 채택. 방향/지속값은 해당 원본 시트 사용 |
| Hop | 10 | 기쁨 등 교류 후보; 종별 지원 검사 후 채택 |
| Nod | 22 | 인사 후보; 시선 방향과 함께 유한 재생 |
| Laying | 27 | 일부 수면 장면에서 사용하는 누운 자세; 일반 Sleep과 구분 |
| Shock | 4 | 이름만 보고 친근한 놀람으로 확정하지 않음. 원본 공격 계열 의미·픽셀 확인 필요 |

지원 상태는 exact_rom, exact_public, alias_exact, declared_fallback, missing, malformed, original_mismatch로 구분한다. exact_public은 공개 원본이 확인됐다는 뜻이며 ROM 픽셀 동일성은 별도 필드로 둔다. 누락된 Sleep을 Idle로 재생하면서 원작 수면 지원으로 표시하지 않는다.

## 4. 실제 공개 샘플 검사

이전 조사에서 PNG signature/IHDR를 확인했고, 이번 작성 시 Pikachu 8개 Anim PNG를 Pillow 12.3.0으로 끝까지 디코딩해 XML 셀 크기·duration 개수·방향 행 수·양수 tick을 검사했다. 모두 통과했다. 마커 좌표 검사, ROM 픽셀 동일성·시각 QA, 모든 폼의 지원 검사는 미실행이다.

| Pikachu 동작 | PNG 크기 | 셀 | 열 × 행 | 지속 tick 목록 |
| --- | --- | --- | --- | --- |
| Walk | 128×320 | 32×40 | 4×8 | 8,10,8,10 |
| Idle | 240×448 | 40×56 | 6×8 | 40,2,3,3,3,2 |
| Sleep | 64×40 | 32×40 | 2×1 | 30,35 |
| EventSleep | 48×256 | 24×32 | 2×8 | 30,35 |
| Wake | 200×320 | 40×40 | 5×8 | 8,6,4,14,10 |
| Eat | 96×48 | 24×48 | 4×1 | 6,8,6,8 |
| Pose | 96×320 | 32×40 | 3×8 | 12,2,8 |
| Nod | 96×384 | 32×48 | 3×8 | 6,8,6 |

8개 시트를 모두 RGBA로 펼친 payload 합은 1,198,080bytes(약1.14MiB)다. 이는 그림 buffer 계산값이며 Pillow/WIC/앱 전체 메모리 실측은 아니다.

Bulbasaur XML의 예: Walk 셀40×40/tick 4를 6개, Eat 24×32/[6,8,6,8], Pose 24×32/[8,1,3,2,8], Wake 32×24/[8,4,12,4,10,4]. 동일 동작이라고 셀 크기·프레임 개수·duration을 종 공통으로 고정하면 안 된다.

## 5. 프레임·오프셋·자동 분할

[skytemple_files/graphics/chara_wan/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/chara_wan/model.py)의 SequenceFrame은 frameIndex,duration,flag,offset,shadow를 저장하며 IsRushPoint/IsHitPoint/IsReturnPoint가 있다. FrameOffset은 head,lhand,rhand,center를 갖는다. 원본 WAN의 메타프레임 조각을 직접 재조합하는 코드를 새로 쓰기보다 CharaWanHandler.export_sheets를 먼저 사용한다.

[skytemple_files/graphics/chara_wan/utils.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/chara_wan/utils.py)의 getOffsetFromRGB는 offset 시트의 채널 마커를 읽는다. 검정=head, 빨강=왼손, 초록=center, 파랑=오른손이며 겹친 마커는 채널별 판정이 필요하다. 순수 빨강/초록/파랑 픽셀만 찾으면 중첩 마커를 누락할 수 있다. Shadow 시트의 흰 마커와 실제 그림자 픽셀을 구분한다.

sheets.py의 DRAW_CENTER_X=0, DRAW_CENTER_Y=-4이므로 셀 기준점은 (width//2,height//2+4)이다. 이미지의 비투명 bounds만 잘라서 발 위치를 잃지 않도록 기준점·head/hand/center·그림자 위치를 동일 crop 원점으로 변환한다. 동작/방향별 프레임 흔들림은 anchor 보존으로 막는다.

자동 분할 절차:

1. tracker/ROM MD 목록에서 필요한 원본 개체를 결정하고 XML의 CopyOf를 재귀 해석한다. resolvedName,원본Index,원본파일을 유지한다.
2. PNG.width%FrameWidth=0, height%FrameHeight=0, columns=durationCount, rows가 지원값인지 검사한다. 0/음수 duration과 범위 밖 이벤트 마커를 오류로 남긴다.
3. 각 (row,column)의 rectangle을 나눈다. 팔레트 transparency를 보존하고 PNG를 RGBA로 디코드한다.
4. 공통 crop 정책으로 프레임·anchor·effect 접점을 정규화한다. 동일 프레임은 hash로 deduplicate할 수 있으나 서로 다른 anchors와 durations는 따로 저장한다.
5. 최종 PNG atlas + animations manifest + source/credits + support report를 출력한다. 이것은 신규 오프라인 packer이며 현재 구현되어 있지 않다.

전수 감사는 tracker/XML 메타데이터부터 수행하고, 필요한 동작의 PNG만 가져온다. 전체 SpriteCollab PNG를 먼저 내려받을 필요는 없다. 품종별 audit 행에는 ROM mdIndex/spriteIndex,collabPath,동작,Index,CopyOf,행/열,ticks,markerValidity,credit,status,fallbackReason을 기록한다.

## 6. 원작 타이밍으로 재생하는 연결

tick 정수를 그대로 저장하고 런타임의 tickHz를 별도 파라미터로 둔다. elapsed를 누적 tick 경계로 찾아 해당 프레임을 재생한다. 프레임 duration마다 다음 변경 시각을 예약하고 같은 프레임 동안 다시 그리지 않는다. 유한 동작은 마지막 tick 종료 시 완료 이벤트를 한 번 발생시키며, 수면 등 루프 동작의 종료는 생활 상태 머신이 결정한다.

SpriteBot의 [SpriteUtils.py](https://github.com/PMDCollab/SpriteBot/blob/8a502bf4553b816d6d3bc2c0d5f2bfd8550690ef/SpriteUtils.py) / animateFileZip는 GIF 미리보기에서 duration×20ms를 사용한다. 이를 원작 실행 속도의 증거로 쓰지 않는다. 명목 60Hz는 개발 초기 가정이며 원작/emulator의 실제 wall time과 비교해 확정해야 한다. 원본 tick, 스크립트 속도 플래그, 이동 속도를 독립적으로 보존한다.

새 런타임 Animator 입력은 assetKey,animationId,directionRow,loopMode,speedMode,startTime이며 출력은 frame rectangle,anchor/shadow,marker events,finished이다. 이 이름은 인터페이스 제안이며 기존 오픈소스 API가 아니다. 이동 보간은 Navigator/Explorer에서 처리하고 Walk 프레임 재생과 분리한다.

## 7. 용량·라이선스·검증 기준

런타임에는 선택한 포켓몬/폼의 필수 동작, 방문 던전 타일셋, 아이템, 생활 배경만 포함한다. 공개 Offsets/Shadow 디버그 시트는 manifest로 변환한 뒤 배포 패키지에서 제외 가능하다. 같은 atlas·팔레트·그림자 deduplicate, PNG 압축 및 문자열 중복 제거를 적용하되 decoded RGBA 캐시 크기도 별도 측정한다. 압축 PNG 총합만으로 RAM 예산을 평가하지 않는다.

SpriteCollab의 [README.md](https://github.com/PMDCollab/SpriteCollab/blob/e4b1949687cb2344e6675717466e357189d3acba/README.md) 및 [LICENSE.md](https://github.com/PMDCollab/SpriteCollab/blob/e4b1949687cb2344e6675717466e357189d3acba/LICENSE.md)에서 제출물 정책과 CC BY-NC 계열 조건을 확인해야 한다. 조사한 공개 정책은 CC BY-NC 4.0 및 기여자 credit을 요구하는 방향이다. CHUNSOFT/CUR/Unspecified 등 기존 크레딧은 기원 정보이며 원작 권리의 별도 이용 허락을 뜻하지 않는다. 루트 LICENSE.md의 CC BY-NC 4.0 본문을 이번 작성 시 다시 확인했다. 코드 라이선스와 그림별 조건은 구분한다.

검증 기준: ROM→WAN merge→시트 출력 성공, 대표 동작의 프레임별 픽셀/alpha/anchor 확인, 8방향 동작 방향 검증, CopyOf 왕복, 원본 타이밍 비교, 전체 개체 지원표 생성, chosen subset의 설치/decoded 크기 실측. 현재는 코드·공개 샘플 구조와 PNG 디코드 확인까지이며 원작 자산 전체 지원·20MB 달성은 미확인이다.
