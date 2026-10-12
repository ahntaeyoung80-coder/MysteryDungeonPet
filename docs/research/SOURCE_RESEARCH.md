# Adaptive Desktop Pet — 채택 소스·함수·API

작성 기준: 2026-10-13 KST. Phase 1~5의 결론을 통합한 코드 선택표다. 원작 ROM 파서는 오프라인 직접 사용, 소형 Windows 앱은 C++17/Win32로 신규 구현한다. 후보의 코드 존재·공개 샘플 검사는 확인했지만 ROM 전체 실행·Windows 성능 검증은 미완료다.

## 1. 고정된 조사 소스

| 키 | 저장소 | 고정 커밋 | 라이선스·범위 | 채택 범위 |
| --- | --- | --- | --- | --- |
| STF | SkyTemple/skytemple-files | 5ed359a2560a1f04666b9ea02b71649b89cb16d7 | GPL-3.0-or-later | 오프라인 ROM 파서·WAN/배경/아이템 변환 |
| ST | SkyTemple/skytemple | f78d0ef387dbc121bad4c8f0186237f9a4ae035d | GPL-3.0-or-later | 그룹 층 인덱싱·분포 계산·자산 참조 기준 |
| SC | PMDCollab/SpriteCollab | e4b1949687cb2344e6675717466e357189d3acba | LICENSE.md CC BY-NC4.0; 개별 credit/원작 기원 별도 | 시트 포맷·지원 감사·보완 후보 |
| SB | PMDCollab/SpriteBot | 8a502bf4553b816d6d3bc2c0d5f2bfd8550690ef | GPL-3.0 | XML/시트/트리 검사 기준; 전체 봇 미도입 |
| ES | SkyTemple/explorerscript | c7728b7225490f3f9448f9770ae9b1e5de00db52 | MIT | 오프라인 SSB decompile |
| PRET | pret/pmd-sky | fc85040831b248ecb690a71898010fe837091613 | 전체 라이선스 확정 안 함; include/LICENSE.txt의 MIT 범위는 복사한 정보만 | 실제 원작 테이블·어셈블리·공개 장면 참고 |
| DBG | UsernameFodder/pmdsky-debug | c72fe675919ec83cfe93f687973afae85e00d1a3 | 루트 GPL-3.0, headers/LICENSE.txt 해당 디렉터리 MIT | API·구조·주소 의미 참고 |
| DM | EpicYoshiMaster/dungeon-mystery | a23726d8fb0038668bfb1f3119eb3db6ecf57d98 | GPL-3.0-or-later | 지도·계단 검사 알고리즘 참고, 직접 이식 미채택 |
| BOT | johanngan/pmd-bot | 8f068e73a676dfa3e0d74cb4626f0480f7bb5668 | MIT; Jumper 수정판 별도 | 탐색·목표·복구 정책 참고 |
| MS | microsoft/Windows-classic-samples | 434f6002bdf9cf9829406c3ff2b33387982d6168 | MIT | WIC/DIB 코드 일부 이식 |
| RUST | microsoft/windows-rs | d5b6a1f68f4fdd0c79495954d162901ea90da4f4 | MIT 또는 Apache-2.0 | Rust 대안 비교; 기본 런타임 미채택 |

이 문서의 GitHub 코드 링크는 모두 해당 커밋에 고정한다. 같은 이름의 최신 브랜치 코드와 섞지 않는다. 파일 범위별 라이선스가 있으면 루트 조건 하나로 덮어쓰지 않는다.

## 2. 실제 채택·참고 함수

각 행의 모드는 직접 사용(오프라인), 일부 이식, 참고, 신규 중 하나다. 실제 API와 제안 인터페이스를 별도로 적는다. 라이선스는 위 저장소 키의 범위를 따른다.

| ID / 기능 | 추천 소스와 정확한 파일 | 함수·API / 입력 → 출력 | 적용·변환·연결 | 의존성·비용 / 확인 수준 |
| --- | --- | --- | --- | --- |
| S01 ROM 판본 | [skytemple_files/common/util.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/common/util.py) | get_ppmdu_config_for_rom(ROM) → Pmd2Data; get_binary_from_rom(ROM,section) → bytearray | 직접 사용. config+ROM 보완 데이터를 모든 파서에 전달 | Python/ndspy/설정; 추출 시만. 실제 본문 재확인, ROM 실행 안 함 |
| S02 던전 정의 | [skytemple_files/hardcoded/dungeons.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/hardcoded/dungeons.py) | HardcodedDungeons.get_dungeon_list(arm9,cfg) → DungeonDefinition[] | 직접 사용. mappa_index/start_after/localFloor 연결, 도장 별도 | ARM9 판본별 block; source 확인 |
| S03 층 표 | [skytemple_files/dungeon_data/mappa_bin/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/dungeon_data/mappa_bin/handler.py), [skytemple_files/dungeon_data/mappa_bin/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/dungeon_data/mappa_bin/protocol.py) | MappaBinHandler.deserialize → MappaBinProtocol.floor_lists | 직접 사용. raw layout/spawn 분포를 정규화 catalogue로 저장 | SIR0/hybrid model; 오프라인 only; ROM 미실행 |
| S04 검증 | [skytemple_files/dungeon_data/mappa_bin/validator/validator.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/dungeon_data/mappa_bin/validator/validator.py) | DungeonValidator.validate(dungeons) → bool+errors | 직접 사용 후보. 그룹·범위 검사와 도장/참조 검사 추가 | mappa/definitions; 전체 실제 실행 미실행 |
| S05 확률 | [skytemple/module/dungeon/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/module.py), [skytemple/module/dungeon/widget/floor.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/widget/floor.py) | calculate_relative_weights(list[int]) → list[int]; _split_items_in_list_in_cats → dict | 참고/일부 이식 후보. CDF 차분과 category 조건부 item 분포; guaranteed 별도 | GUI 의존 코드는 가져오지 않음; 정수 계산은 추출 시. 원본 함수 본문 격리 실행5사례 통과 |
| S06 MD/item | [skytemple_files/data/md/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/data/md/handler.py), [skytemple_files/data/item_p/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/data/item_p/handler.py) | MdHandler.deserialize,ItemPHandler.deserialize → 레코드 객체 | 직접 사용. 원본 ID·폼·sprite/palette 참조 유지 | hybrid/SIR0; 오프라인 only; source 확인 |
| S07 문자열 | [skytemple_files/data/str/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/data/str/handler.py) | StrHandler.deserialize → strings | 직접 사용. language filename/StringBlocks 인덱스 연결 | 원본 encoding; 오프라인 only |
| S08 dungeon pack | [skytemple_files/container/dungeon_bin/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/container/dungeon_bin/handler.py), [skytemple_files/container/dungeon_bin/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/container/dungeon_bin/model.py) | DungeonBinHandler.deserialize(data,static_data) → DungeonBinPack; get/get_raw | 직접 사용. 이름은 config에서 복원; 특정 slot만 변환 | BIN_PACK/compression/Pillow; source 확인 |
| S09 타일 | [skytemple_files/graphics/dma/dma_drawer.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/dma/dma_drawer.py), [skytemple_files/graphics/dpc/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/dpc/protocol.py) | get_mappings_for_rules → 청크 grid; draw/chunks_to_pil → PIL frames | 오프라인 직접 사용. 원작 autotile rule/atlas는 새 지도에 연결 | DPC/DPCI/DPL/DPLA; 층 변경 시·사전변환. ROM 렌더 미실행 |
| S10 item icon | [skytemple_files/graphics/img_itm/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/img_itm/model.py), [skytemple/core/sprite_provider.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/core/sprite_provider.py) | ImgItm.to_pil(sprite,palette); _load_item__impl | 직접 사용/참고. 팔레트 index modulo16 alpha 규칙 유지 | Pillow; 원본 검정색 보존; 소스 확인 |
| S11 배경 | [skytemple/module/map_bg/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/map_bg/module.py), [skytemple_files/graphics/bma/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/bma/protocol.py) | get_bma/get_bpc/get_bpl/get_bpas; to_pil/to_pil_single_layer | 파서 직접 사용, GUI wrapper 참고. 선택 장면만 배경/팔레트 변환 | MAP_BG 파일 묶음; 오프라인 memory 임시 |
| S12 ROM sprite | [skytemple/module/sprite/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/sprite/module.py) | get_monster_*_sprite_chara(spriteIndex) → WanFile | 호출 흐름 참고. monster/attack 압축,ground raw. 정확한 원본 index로 연결 | GUI project는 런타임 미도입; ROM 미실행 |
| S13 WAN 변환 | [skytemple_files/graphics/chara_wan/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/chara_wan/handler.py) | deserialize,merge_wan,export_sheets,import_sheets,split_wan | 직접 사용. 필수 동작만 atlas로 후처리, anchor/tick 보존 | Python/Pillow; strict·픽셀 왕복 미실행 |
| S14 시트 marker | [skytemple_files/graphics/chara_wan/sheets.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/chara_wan/sheets.py), [skytemple_files/graphics/chara_wan/utils.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/chara_wan/utils.py) | ImportSheets/ExportSheets,getOffsetFromRGB | 참고/일부 이식 후보. 채널 marker,centerY+4,crop 보정 | 오프라인 only; all-marker QA 미실행 |
| S15 공개 지원 감사 | [SpriteUtils.py](https://github.com/PMDCollab/SpriteBot/blob/8a502bf4553b816d6d3bc2c0d5f2bfd8550690ef/SpriteUtils.py), [TrackerUtils.py](https://github.com/PMDCollab/SpriteBot/blob/8a502bf4553b816d6d3bc2c0d5f2bfd8550690ef/TrackerUtils.py) | getStatsFromTree,verifySprite,getFramesAndMappings,iterateTracker/getNodeFromIdx/getDirFromIdx | 참고 또는 오프라인 호출. CopyOf·폼 트리·credit 상태 기록 | 봇 전체 기동 불필요; 2종 XML/8PNG 구조·전체 디코드 확인 |
| S16 스크립트 | [skytemple_files/script/ssb/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssb/handler.py), [skytemple_files/script/ssb/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssb/model.py) | SsbHandler.deserialize; Ssb.to_explorerscript/to_ssb_script → (text,SourceMap) | 오프라인 직접 사용. source offset을 생활 cue로 정리 | Pmd2Data+ES; 공개8SSB 경계 검사는 실행, 공식 파서 미실행 |
| S17 원본 배치 | [skytemple_files/script/ssa_sse_sss/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssa_sse_sss/model.py), [skytemple_files/script/ssa_sse_sss/position.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/script/ssa_sse_sss/position.py) | Ssa.layer_list/triggers/position_markers,SsaPosition.x/y_absolute | 직접 사용. actorId/objectId/좌표/방향 정규화 | scriptdata; 공개1SSA 구조 검사 실행 |
| S18 ID 매핑 | [asm/overlay_11_022ECD24_data.s](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/asm/overlay_11_022ECD24_data.s), [asm/overlay_11_022EE5E4.s](https://github.com/pret/pmd-sky/blob/fc85040831b248ecb690a71898010fe837091613/asm/overlay_11_022EE5E4.s) | SETANIMATION_TABLE,ChangeActorAnimation | 데이터/의미 참고. script code→word→WAN/loop/speed/restart | 원작 ASM 직접 실행/포팅 안 함; 84entry 추출 검사 |
| S19 효과 | [skytemple_files/graphics/effect_wan/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/effect_wan/handler.py), [skytemple_files/graphics/effect_wan/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/effect_wan/model.py) | EffectWanHandler.deserialize/export_sheets | 오프라인 사용 후보. format 예외와 effect id→slot 실제 연결 확인 후 채택 | 효과642/643/624 실제 graphic 슬롯 미확인 |
| S20 경량 생성 | [src/dungeon.ts](https://github.com/EpicYoshiMaster/dungeon-mystery/blob/a23726d8fb0038668bfb1f3119eb3db6ecf57d98/src/dungeon.ts), [src/random.ts](https://github.com/EpicYoshiMaster/dungeon-mystery/blob/a23726d8fb0038668bfb1f3119eb3db6ecf57d98/src/random.ts) | CreateRoomsAndAnchors/CreateHallway/SpawnStairs/StairsAlwaysReachable; RandInt/RandRange | 참고. 전역 상태·원작 특수층 제거하고 새2~3방 generator | 추가 dependency 없음; 신규 geometry10k 통과, upstream 미실행 |
| S21 자율 탐색 | [agent/SimpleAgent.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/agent/SimpleAgent.lua), [agent/Agent.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/agent/Agent.lua), [utils/pathfinder.lua](https://github.com/johanngan/pmd-bot/blob/8f068e73a676dfa3e0d74cb4626f0480f7bb5668/utils/pathfinder.lua) | act,setTargetPos,findTargetPath,getPath,exploreLayout | 정책 참고. 목표/경로 캐시/차단 복구를 native BFS로 신규 구현 | emulator/Jumper 미포함; 원본 bot 실행 미실행 |
| S22 PNG·DIB | [Samples/Win7Samples/multimedia/wic/wicviewergdi/WicViewerGdi.cpp](https://github.com/microsoft/Windows-classic-samples/blob/434f6002bdf9cf9829406c3ff2b33387982d6168/Samples/Win7Samples/multimedia/wic/wicviewergdi/WicViewerGdi.cpp) | DemoApp::CreateDIBFromFile/ConvertBitmapSource/CreateDIBSectionFromBitmapSource → HRESULT/DIB | 일부 이식. BGR→PBGRA,Fant→nearest,dialog 제거,frame cache | WIC/Ole32/Gdi32; 소스 확인, 실제 Windows 미실행 |

실제 export API는 CharaWanHandler.export_sheets이고 sheets.py의 ImportSheets/ExportSheets는 함수다. DungeonModule.get_mappa_floor의 floor_id는 GUI의 그룹 기준 인덱스라는 점을 지킨다. getStatsFromTree 등이 있다고 전체 폼 감사가 끝난 것은 아니다.

## 3. Windows API 연결표

정확한 호출·오류·측정 기준은 [05_WINDOWS_RUNTIME.md](05_WINDOWS_RUNTIME.md)에 있다. 아래 API는 OS 제공 API이고 신규 프로젝트 함수로 혼동하지 않는다.

| 기능 | API | 주요 input/output | 의존성과 비용 |
| --- | --- | --- | --- |
| popup/alpha/topmost | CreateWindowExW,UpdateLayeredWindow,SetWindowPos | style+DIB+blend → HWND/BOOL | User32/Gdi32; 변화 시만 surface 제출 |
| WIC decode | CreateDecoderFromFilename,GetFrame,CreateFormatConverter,CopyPixels | path/frame/PBGRA format → 픽셀 | Windowscodecs/Ole32; cache miss에만 decode |
| active app | SetWinEventHook,GetForegroundWindow,GetWindowThreadProcessId,OpenProcess,QueryFullProcessImageNameW | foreground event → PID/path 또는 Unknown | User32/Kernel32; callback 짧게, handle close |
| idle | GetLastInputInfo,GetTickCount | LASTINPUTINFO → 세션별 last tick | User32; 1~5초 check 제안 |
| lock/power/display | WTSRegisterSessionNotification,WM_WTSSESSION_CHANGE,WM_POWERBROADCAST,RegisterPowerSettingNotification | OS event → activity state | Wtsapi32/User32; lock/off 때 렌더 취소 |
| deadline | CreateWaitableTimerExW,SetWaitableTimerEx,MsgWaitForMultipleObjectsEx | 다음 시각+message mask → wake | Kernel32/User32; busy loop 금지 |
| persistence | SHGetKnownFolderPath,CreateFileW,WriteFile,FlushFileBuffers,MoveFileExW | versioned snapshot → local state | Shell32/Kernel32; debounce/오류 복구 |
| measurement | GetProcessTimes,K32GetProcessMemoryInfo | process → CPU times/WS/private bytes | Kernel32; 실측 미완료 |

## 4. 신규 구현 범위

Catalogue/AssetStore/WorldGenerator/SpawnSampler/Navigator/Explorer/Animator/EventPlayer/Lifecycle/ActivityMonitor/WindowHost/LayeredRenderer/StateStore는 신규 모듈 제안이다. 현재 프로젝트에 구현되어 있지 않다. 기존 GUI/원작 VM/emulator 코드를 실행하는 glue만 만드는 것으로 제품이 완성되지 않으며, 위 작은 네이티브 모듈이 필요하다.

기존 데이터→정규화 catalogue→asset manifest→runtime snapshot 계약은 [ASSET_MAPPING.md](ASSET_MAPPING.md)와 [IMPLEMENTATION_HANDOFF.md](IMPLEMENTATION_HANDOFF.md)를 사용한다. 코드 채택 여부와 resource availability를 별도 관리한다.

## 5. 확인 수준 요약

확인: 실제 파일·함수/클래스·테이블, 공개 XML/PNG 구조·8PNG 전체 디코드,CDF 원본 함수 격리 실행5사례,8SSB/1SSA 독립 경계 검사,신규 geometry10,000 seeds. 미실행: 공식 ROM 전체 추출/왕복,전체 종·폼/동작 감사,원작 tick wall time 대조,효과/계단 graphic 매핑,실제 Windows 빌드·창·활동·전원·성능·설치 크기. 해당 미확인은 개발 명세의 선행 검증 gate이며 이미 확인한 코드 조사 전체를 다시 시작할 사유는 아니다.
