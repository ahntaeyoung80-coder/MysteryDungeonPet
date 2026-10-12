# Phase 3 — 원작 이벤트 및 생활 장면

작성 기준: 2026-10-13 KST. 원작 이벤트 VM 전체를 도입하지 않고 필요한 스크립트·배치·애니메이션·효과를 추출해 유한한 생활 이벤트로 재생한다. 원작 코드 존재, 공개 바이너리 구조 검사, 실제 게임 실행을 구분한다. 모든 장면의 실제 화면 재현은 아직 검증하지 않았다.

## 1. 조사 소스와 역할

| 저장소·커밋 | 실제 파일·API | 사용 방식·라이선스 |
| --- | --- | --- |
| SkyTemple/skytemple-files / 5ed359a2560a1f04666b9ea02b71649b89cb16d7 | [skytemple_files/script/ssb/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssb/handler.py) / SsbHandler.deserialize(data,static_data); [skytemple_files/script/ssb/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssb/model.py) / Ssb | 오프라인 파서 직접 사용 후보. GPL-3.0-or-later; ROM 파서 실행 미실행 |
| 같은 저장소 | [skytemple_files/script/ssa_sse_sss/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssa_sse_sss/handler.py) / deserialize(data,scriptdata); [skytemple_files/script/ssa_sse_sss/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssa_sse_sss/model.py) / Ssa | SSA/SSE/SSS 배치와 스크립트 연결을 추출 |
| SkyTemple/explorerscript / c7728b7225490f3f9448f9770ae9b1e5de00db52 | [explorerscript/ssb_converting/ssb_decompiler.py](https://github.com/SkyTemple/explorerscript/blob/c7728b7225490f3f9448f9770ae9b1e5de00db52/explorerscript/ssb_converting/ssb_decompiler.py) / ExplorerScriptSsbDecompiler.convert | 이벤트를 사람이 읽을 수 있는 텍스트로 변환하는 오프라인 도구. MIT |
| pret/pmd-sky / fc85040831b248ecb690a71898010fe837091613 | [asm/overlay_11_022EE5E4.s](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/asm/overlay_11_022EE5E4.s) / ChangeActorAnimation; [asm/overlay_11_022ECD24_data.s](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/asm/overlay_11_022ECD24_data.s) / SETANIMATION_TABLE | 실제 어셈블리·데이터 테이블을 의미 확인에 참고. 전체 게임 엔진 이식 안 함 |
| UsernameFodder/pmdsky-debug / c72fe675919ec83cfe93f687973afae85e00d1a3 | [headers/functions/overlay11.h](https://github.com/UsernameFodder/pmdsky-debug/blob/c72fe675919ec83cfe93f687973afae85e00d1a3/headers/functions/overlay11.h), [symbols/overlay11.yml](https://github.com/UsernameFodder/pmdsky-debug/blob/c72fe675919ec83cfe93f687973afae85e00d1a3/symbols/overlay11.yml), [headers/types/ground_mode/ground_mode.h](https://github.com/UsernameFodder/pmdsky-debug/blob/c72fe675919ec83cfe93f687973afae85e00d1a3/headers/types/ground_mode/ground_mode.h) | 함수 선언·주소·구조 의미 참고. 저장소 루트 LICENSE.txt는 GPL-3.0; headers/LICENSE.txt는 해당 디렉터리에만 MIT, symbols도 별도 범위 확인 필요 |

pret/pmd-sky 전체를 MIT라고 표기하지 않는다. [include/LICENSE.txt](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/include/LICENSE.txt)의 MIT 문구는 pmdsky-debug에서 복사한 해당 정보에 한정된다. 공개 바이너리·원작 이미지의 권리와 코드·주석의 라이선스는 별도로 기록한다. 이 결과물에는 원작 SSB/SSA/이미지 파일을 포함하지 않는다.

## 2. 파서 호출과 데이터 연결

SsbHandler는 판본 설정에 따라 SsbHeaderUs(12바이트), SsbHeaderEu(18), SsbHeaderJp(12)를 선택한다. 헤더를 항상 12바이트로 가정하는 독립 검사 코드는 이번 공개 US 샘플에만 적용했다. 실제 추출 도구는 Phase 1의 Pmd2Data와 공식 파서를 사용한다.

Ssb는 routine_info,routine_ops,constants,strings와 get_filled_routine_ops를 제공한다. to_explorerscript(),to_ssb_script()는 (text,SourceMap)을 반환하므로 원본 루틴·명령 offset과 텍스트 위치를 연결할 수 있다. 이 반환형은 이번 작성 시 함수 정의에서 다시 확인했다.

SSA 계열은 layer_list,position_markers,triggers 및 actor/object/performer/event 정보를 갖는다. 모델의 header/layer pointer는 16비트 word 단위이므로 실제 byte 위치로 ×2 한다. layer 레코드는 20바이트, actor는 16바이트, object는 20바이트이다.

| actor 레코드 offset | 내용 | object 레코드 offset | 내용 |
| --- | --- | --- | --- |
| +0,+2 | actorId,SSA direction | +0,+2 | objectId,direction |
| +4,+6 | x,y 타일 좌표 | +4,+6 | hitbox width,height |
| +8,+0xA | x/y offset | +8,+0xA | x,y 타일 좌표 |
| +0xC,+0xE | signed scriptId,unknown | +0xC,+0xE | x/y offset |
| — | — | +0x10,+0x12 | signed scriptId,unknown |

[skytemple_files/script/ssa_sse_sss/position.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssa_sse_sss/position.py) / SsaPosition의 실제 계산은 tile*8 + (offset>=2이면 4, 아니면 0)이다. raw offset을 그대로 픽셀에 더하지 않는다. actorId는 layer 배열 인덱스와도 다르다. 스크립트의 lives actor 식별자는 원본 배치·설정과 연결해 해석한다.

[skytemple_files/script/ssb/ssb_number.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssb/ssb_number.py) / parse_ssb_encoding은 Number의 부호·고정소수점 인코딩을 해석한다. Number 인수를 모두 unsigned u16으로 써서 위치·속도가 잘못되지 않도록 공식 파서의 값을 사용한다. 이번 경계 검사에서 나열한 정수는 확인한 명령의 raw 인수이며 모든 Number 의미를 실행한 것은 아니다.

## 3. 명령과 스크립트 ID → WAN ID

[skytemple_files/_resources/ppmdu_config/pmd2scriptdata.xml](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/_resources/ppmdu_config/pmd2scriptdata.xml)에서 실제 명령 정의를 확인했다.

| 명령 / opcode | 인수 | 오프라인 출력·경량 대체 |
| --- | --- | --- |
| SetAnimation / 0xFD | Number id | 원본 scriptAnimationCode를 테이블·flag로 해석한 뒤 WAN ID로 정규화 |
| SetDirection / 0x100 | direction | SSB 0..7 → atlas row 0..7 |
| SetEffect / 0x102 | Effect effect_id, Number position_marker_id? | effect ID·접점 raw 값 보존. 두 번째 인수의 의미가 XML에서도 물음표이므로 확정하지 않음 |
| SetPosition / 0x108 | Number x,y | 원본 좌표 → 장면 공통 원점·화면 배율로 변환 |
| Wait / 0x157 | Number duration | 원본 tick 대기 cue |
| WaitAnimation / 0x158 | 없음 | 원작 애니메이션 대기 cue; 유한 완료·루프 종료 정책은 원작 실행 확인 필요 |
| WaitEndAnimation / 0x15F | 없음 | 별도 완료 대기 명령; WaitAnimation과 무조건 동일 구현으로 간주하지 않음 |

SetAnimation(64)는 WAN 애니메이션 64가 아니다. ChangeActorAnimation(NA 0x022F93C8)은 low byte 테이블 조회와 speed/special/restart 플래그를 처리한다. SETANIMATION_TABLE은 84개의 little-endian u16 animation_data이다. low byte는 WAN ID, high byte에는 속도·loop·추가 flag가 있다.

| script code | 테이블 word | WAN ID / 동작 |
| --- | --- | --- |
| 6 | 0x0805 | 5 / Sleep, loop flag |
| 22 / 42 / 62 | 0x100D / 0x080D / 0x080D | 13 / EventSleep |
| 23 / 43 / 63 | 0x100E / 0x080E / 0x100E | 14 / Wake |
| 24 / 44 / 64 | 0x100F / 0x080F / 0x080F | 15 / Eat |
| 26 / 46 / 66 | 0x1011 / 0x0811 / 0x1011 | 17 / Pose |
| 31 / 51 / 71 | 0x1016 / 0x0816 / 0x1016 | 22 / Nod |
| 76 | 0x081B | 27 / Laying |

1/2는 특수 idle 선택 경로로 GetIdleAnimationType 결과를 사용한다. freeze Walk(0x0300), loop Idle(0x0807), freeze Idle(0x0307) 같은 변형이 있어 1/2를 WAN Index로 곧바로 해석하지 않는다.

원본 코드의 0x700 속도 override는 테이블 high byte 속도 값과 같은 수치가 아니다. 테이블에 속도 bits가 없을 때 override 0x100→freeze(0x300),0x200→slow(0x100),0x300→normal(0),0x400→fast(0x200) 계열로 처리한다. 0x1000은 restart와 관계되고 0x800 특수 경로의 전체 의미는 미확인이다. 구현은 raw word/flags와 해석 결과를 모두 남기고 지원하지 않는 플래그를 조용히 삭제하지 않는다.

pmdsky-debug의 ChangeActorAnimation,SetAnimationLiveActor,GetAnimationStatusLiveActor,SetEffectLiveActor는 선언·심볼의 존재를 확인한 API이다. ChangeActorAnimation의 본문은 pret 어셈블리로 확인했지만 모든 선언의 C 구현이 존재한다고 주장하지 않는다.

## 4. 공개 원작 파일에 대한 실행한 검사

이번 MD 작성 시 공개 US SSB 8개와 SSA 1개를 다시 읽고 독립 Python 검사기를 실행했다. XML opcode별 인수 개수로 각 루틴의 byte 경계를 검사하고, 중복 offset은 alias로 처리했다. 공식 SsbHandler/ExplorerScript 실행이나 게임 플레이 검증은 아니다.

모든 파일 경로는 pret/pmd-sky의 files/language-specific/US/SCRIPT/ 아래다.

| 실제 파일 | bytes | 루틴 수 | 고유 루틴 opcode 수 | 확인된 용도 |
| --- | --- | --- | --- | --- |
| [files/language-specific/US/SCRIPT/COMMON/unionall.ssb](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/COMMON/unionall.ssb) | 71082 | 701, 고유692 | 8094 | 공통 수면·기상 루틴 |
| [files/language-specific/US/SCRIPT/S01P02A/conquest.ssb](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/S01P02A/conquest.ssb) | 404 | 5 | 41 | 완료/귀환 문맥. Pose 명령 확인 자료로 사용하지 않음 |
| [files/language-specific/US/SCRIPT/S01P02A/return.ssb](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/S01P02A/return.ssb) | 690 | 5 | 49 | 귀환 배경·대화/idle 흐름 |
| [files/language-specific/US/SCRIPT/G01P06B/c00a0601.ssb](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/G01P06B/c00a0601.ssb) | 1096 | 2 | 97 | 식사 장면·효과 |
| [files/language-specific/US/SCRIPT/G01P06B/n01a0601.ssb](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/G01P06B/n01a0601.ssb) | 848 | 2 | 73 | 식사 장면의 다른 버전 |
| [files/language-specific/US/SCRIPT/G01P07A/c00a0201.ssb](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/G01P07A/c00a0201.ssb) | 874 | 1 | 119 | 누운 자세·기상 |
| [files/language-specific/US/SCRIPT/G01P07C/c00a0301.ssb](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/G01P07C/c00a0301.ssb) | 452 | 1 | 26 | 수면 공간 설정 |
| [files/language-specific/US/SCRIPT/G01P07C/m02a0113.ssb](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/G01P07C/m02a0113.ssb) | 3490 | 1 | 129 | Laying/EventSleep·대기 |
| [files/language-specific/US/SCRIPT/G01P06B/c00a0601.ssa](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/files/language-specific/US/SCRIPT/G01P06B/c00a0601.ssa) | 286 | 1 layer | 12 actor,1 object | 좌표·방향·소품 연결 |

재현 방법: US header=12 bytes, constStart=12+u16@12*2, routineCount=u16@14, routine table=16+6*i, offset 단위는 (byte-12)/2. 인수 수가 -1인 opcode는 뒤의 u16에서 개수를 읽는다. 각 고유 루틴은 다음 고유 offset 또는 constStart에서 정확히 끝나야 한다. 이 검사기를 추출 엔진 대신 쓰지 않는다. 정식 추출·지역별 로딩은 공식 파서를 호출한다.

## 5. 장면별 원작 데이터와 경량 cue

### 식사

G01P06B/c00a0601.ssb는 back_SetGround(210)을 호출한다. 상대 word offset 26/30에서 주인공·동료 맥락의 SetAnimation(64)을 확인했다. 다른 NPC는 43/42 같은 Wake/EventSleep을 조합하고 효과를 붙이므로 모든 식사 참여자를 Eat 하나로 묶지 않는다.

SetEffect(642,3)=Eating 계열, (643,3)=EatingSlow 계열, actor79 맥락의 (624,3)=AppleOnHead를 확인한 조사 결과가 있다. 원본은 Wait(20) 등을 사용한다. 효과 ID는 이름/설정과 함께 남기되 EFFECT pack의 동일 번호 슬롯이라고 추측하지 않는다. 642/643/624의 최종 실제 graphic slot 연결은 미확인이다.

SSA에서 actorId0=(220,184), actorId10=(196,184), 방향 SSA1=Down이다. 배열 인덱스는 각각 1/0이므로 actorId와 혼동하지 않는다. objectId107=(256,212), 방향 SSA1이며 설정 object name g01p06b1 → GROUND/g01p06b1.wan으로 연결한다.

경량 흐름: 귀가+음식 보유 → 원본 Eat/WAN15와 지정 방향 → 원본 효과·음식 소품(확인된 자산만) → 유한 재생/원본 tick 대기 종료와 once 소비 → Sleep 준비. 식사량·소비 시점·주기 결정은 신규 생활 정책이며 원작 이벤트 확률로 주장하지 않는다.

### 수면

COMMON/unionall.ssb routine22, offset2590: SetAnimation(768=0x300) → SetAnimation(6) → Wait(240) → Jump(2594). 이것은 Sleep/WAN5 루프가 끝없이 유지되는 공통 루틴의 근거다.

routine48, offset3026: SetAnimation(62) → SetDirection(2) → Wait(60) → Return. routine49는 Hold로 끝난다. 여기서 방향 2는 SSB Right이며 SSA2와 다르다. G01P07C/m02a0113.ssb는 배경212,SetAnimation76(Laying),62(EventSleep),Wait90/120을 포함한다.

경량 흐름: 식사 종료+수면 시간 → 원본 Sleep5/EventSleep13/Laying27 중 장면·종 지원에 맞는 clip → 배경/그림자 → 생활 수면 deadline까지 유지 → Wake. Sleep 루프 자체의 finished를 기다리면 영원히 깨지 못하므로 생활 상태가 종료를 결정한다.

### 기상

COMMON routine26, offset2630: SetAnimation62 → Wait60 → SetAnimation63 → WaitAnimation → Wait10 → SetAnimation2 → Turn2Direction(4,10,0) → Wait40 → Return. routine27은 동일 계열을 Hold로 끝낸다. routine680의 EVENT_WAKEUP 이름만 보고 별도 기상 애니메이션 루틴이라고 판단하면 안 된다. 실제로 679와 같은 offset을 공유하며 ProcessSpecial/Case/Lock/End 문맥이다.

G01P07A/c00a0201.ssb는 배경211,SetAnimation76 → 63 → WaitAnimation → Wait10 → 2 계열을 보여 준다.

경량 흐름: 수면 deadline+시스템 활성 → 원본 Wake14/방향 → 효과(확인 시) → clip 완료+짧은 원본 대기 → Idle → 재탐험. 절전에서 깨어났다는 OS 알림만으로 즉시 사용자 활동 상태로 바꾸지 않는다.

### 친구와 인사·기쁨

원본 Nod22, Hop10, Idle7과 방향 전환은 확인했지만 특정 인사/놀람 장면의 SSB 및 소품·표정 조합은 아직 고정하지 못했다. Shock4를 생활 놀람으로 확정하지 않는다.

경량 흐름: 우호적 개체를 일정 거리에서 만나고 cooldown 해제 → 서로 바라보기+지원된 Nod/Hop → 선택 효과(원본 확인 시) → 유한 동작 종료 → 양보/탐색 재개. 이는 원본 clip을 쓰는 신규 행동이며 원작 인사 스크립트의 완전 이식으로 표시하지 않는다.

### 아이템 발견·획득

[headers/functions/overlay29.h](https://github.com/UsernameFodder/pmdsky-debug/blob/c72fe675919ec83cfe93f687973afae85e00d1a3/headers/functions/overlay29.h)의 TryLeaderItemPickUp,TryNonLeaderItemPickUp,GetItemInfo,ChangeMonsterAnimation은 원작 던전 실행 의미를 추적하는 선언이다. 특정 발견 효과·획득 동작의 전체 구현·원본 프레임 시퀀스는 미확인이다.

경량 흐름: 새 item instance 접근 → 바라보기/Idle 또는 확인한 동작 → 원본 item icon 표시 → 인벤토리에 once 삽입+지도에서 제거 → 다음 목표. 동일 instance 획득과 음식 소비가 저장/복원 뒤 중복되지 않도록 transaction key를 둔다.

### 던전 클리어·포즈

Pose/WAN17을 원작 클리어 동작으로 채택한다. script code26/46/66의 WAN17 매핑은 실제 테이블에서 확인했다. 조사한 S01P02A/conquest.ssb/return.ssb에서는 Pose 자체를 확정하지 못했으므로 해당 파일을 포즈 재생 명령의 증거로 인용하지 않는다. 원작 클리어 장면의 정확한 카메라·효과·지연 조합은 후속 확인 항목이다.

경량 흐름: 마지막층 계단 도달 → 원본 Pose17 → 확인된 클리어 효과/배경 → 유한 포즈 종료 → ReturnHome → Eat 또는 Sleep. 원본 동작은 유지하고 원작 전투/보상/스토리 VM은 도입하지 않는다.

## 6. 배경·효과·표정 추출

| 리소스 | 연결·추출 코드 | 미확인·패키징 |
| --- | --- | --- |
| G01P06B 식사 배경 | level210 → script level.mapid → MAP_BG/bg_list.dat; MAP_BG/g01p06b.bma/.bpc/.bpl, g01p06b1.bpa | Phase 1 MapBgModule과 BMA/BPC/BPL/BPA 파서. 필요한 레이어만 추출 |
| G01P07A/G01P07C 수면 공간 | level211/212 → bg_list; MAP_BG/g01p07a.*,g01p07c.*; GROUND/g01p07a1.wan | script level ID와 bg_list 인덱스를 동일하다고 가정하지 않음 |
| 소품 object107 | Pmd2Data.script_data.objects의 name → GROUND/g01p06b1.wan | actor sprite가 아니라 object WAN이므로 effect/chara format 확인 후 변환 |
| 효과 | [skytemple_files/graphics/effect_wan/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/effect_wan/handler.py) / EffectWanHandler.deserialize,export_sheets | [skytemple_files/graphics/effect_wan/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/effect_wan/model.py)가 효과268..289의 화면 계열,290/291 비SIR0 예외를 언급. 모든 pack entry를 WAN.CHARA로 열지 않음 |
| 초상화 | FONT/kaomado.kao | 대화창 초상화와 필드 몸체/표정 clip은 다른 자산. 필요 장면만 선택 |

pret의 nitrofs_files.txt는 한 줄에 많은 경로를 담고 있어 전체를 출력하지 말고 whitespace split 뒤 필요한 이름만 조회한다. 실제 ROM 파일명 존재와 그 파일의 화면 내용 확인은 별도 단계다.

## 7. 신규 경량 이벤트 구조

새 EventPlayer의 cue 타입은 SetClip,SetDirection,SetPosition,ShowEffect,WaitTicks,WaitClip,ConsumeItem,Transition으로 제한하는 안을 채택한다. 이것은 신규 인터페이스 제안이며 기존 저장소에 구현된 클래스가 아니다. cue에는 sourceScript,routine,opcodeOffset,rawArguments,resolvedClip,confirmedResource,onceKey를 남긴다.

생활 상태는 Explore/ReturnHome/Eat/Sleep/Wake/Ready를 사용하고 이벤트는 이를 일시적으로 점유한다. 렌더는 현재 clip·배경·효과만 받는다. Wait는 busy loop가 아니라 다음 deadline 예약으로 처리한다. 장면 좌표는 원본에서 한 번 정규화하고 100~200px 창에 맞춰 crop/정수 배율을 적용한다. 충돌·경로는 화면 좌표와 분리한다.

검증 필요: 공식 SSB/SSA 파서 실행과 decompile, 원본 clip/효과 접점 픽셀 QA, 원작 wall time과 tick/speed 비교, effect ID 실제 슬롯, 발견/인사/클리어 장면의 세부 cue. 코드·테이블·일부 원작 장면 근거는 확보했으나 모든 생활 디테일이 자동 추출 검증된 상태는 아니다.
