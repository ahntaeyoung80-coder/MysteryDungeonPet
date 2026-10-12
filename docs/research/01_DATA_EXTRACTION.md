# Phase 1 — 원작 데이터 추출

작성 기준: 2026-10-13 KST. 제품 개발은 수행하지 않았다. 이 문서는 원작 ROM을 읽는 오프라인 추출 도구의 코드 선택과 연결 규칙을 확정한다. 실제 ROM 추출·전체 데이터 검사·파서 round-trip은 ROM 부재로 미실행이다.

## 1. 채택과 확인 수준

SkyTemple/skytemple-files의 실제 파서와 압축 해제 코드를 오프라인에서 직접 사용한다. SkyTemple GUI는 데이터 인덱싱·확률 해석·그림 참조의 기준으로만 사용한다. Python, GTK, skytemple-rust를 데스크톱 펫 런타임에 포함하지 않는다.

| 저장소 | 고정 커밋 | 적용 | 라이선스 |
| --- | --- | --- | --- |
| SkyTemple/skytemple-files | 5ed359a2560a1f04666b9ea02b71649b89cb16d7 | 파서 직접 호출 | GPL-3.0-or-later |
| SkyTemple/skytemple | f78d0ef387dbc121bad4c8f0186237f9a4ae035d | GUI 호출 흐름 참고, 일부 변환 로직 이식 후보 | GPL-3.0-or-later |

확인 수준은 소스 확인, 공개 파일 구조 검사, 독립 계산 실행, ROM 실행, Windows 실행으로 구분한다. 함수가 있다는 사실은 해당 ROM에서 성공했다는 뜻이 아니다. 공개 코드의 라이선스는 게임에서 추출한 이미지의 사용 권리를 대신하지 않는다.

## 2. 환경과 ROM 식별

[pyproject.toml](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/pyproject.toml)의 버전은 1.8.5이다. 주요 의존성은 Python >=3.10, ndspy >=3, Pillow >=6.1, range-typed-integers >=1.0.1, appdirs >=1.4, pmdsky-debug-py ==10.0.48, explorerscript >=0.2.2,<0.3, skytemple-rust >=1.8.5,<1.9, dungeon-eos ==0.0.5이다. GUI 전체 설치 대신 이 고정 소스의 별도 가상환경과 lock 파일을 사용한다. 이 문서에서는 설치·import를 실행하지 않았다.

| 기능 | 실제 소스 / 함수 | 입력 → 출력 | 호출·변환 및 연결 | 의존성·비용·검증 |
| --- | --- | --- | --- | --- |
| ROM 열기 | ndspy.rom.NintendoDSRom.fromFile | .nds 경로 → ROM 객체 | 파일은 읽기 전용 취급, 원본 SHA-256·idCode·판본 기록 | ndspy; 전체 ROM 메모리는 추출 시만 사용; ROM 미실행 |
| 판본 선택 | [skytemple_files/common/util.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/common/util.py) / get_ppmdu_config_for_rom | NintendoDSRom → Pmd2Data | idCode와 arm9[0xE:0x10]를 XML 판본에 대조하고 RomDataLoader로 ROM별 설정 보완 | 지원되지 않으면 오류; 실제 함수 본문 확인 |
| 바이너리 추출 | 같은 util.py / get_binary_from_rom | ROM, SectionProtocol → bytearray | config.bin_sections.arm9 또는 overlayN 전달; arm9+arm9PostData, overlay는 loadArm9Overlays 사용 | 바이너리 주소는 판본 설정 사용; 본문 확인 |
| 설정 보완 | [skytemple_files/common/ppmdu_config/rom_data/loader.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/common/ppmdu_config/rom_data/loader.py) / RomDataLoader | ROM → 설정 변경 | BALANCE/actor_list.bin, level_list.bin, objects.bin 및 MONSTER/sprconf.json 등으로 기본 XML 보완 | 이름·분류를 고정 XML만으로 단정하지 않음; 소스 확인 |

[skytemple_files/_resources/ppmdu_config/pmd2data.xml](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/_resources/ppmdu_config/pmd2data.xml)에서 지원 여부를 판본별로 판단한다. NA/EU와 WVC 계열 설정이 존재하며 JP 설정은 이 스냅샷에서 issupported=false이다. 한국어판·개조 ROM은 지원한다고 가정하지 않는다. 언어별 문자열 블록과 바이너리 주소를 NA 값으로 고정하지 않는다.

## 3. 던전 목록과 층 연결

| 데이터 | ROM 위치 / 구조 | 실제 파서·함수 | 추출·연결 방법 |
| --- | --- | --- | --- |
| 기본 던전 정의 | ARM9 DUNGEON_DATA_LIST; 4바이트 레코드 | [skytemple_files/hardcoded/dungeons.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/hardcoded/dungeons.py) / HardcodedDungeons.get_dungeon_list | config의 블록 주소·길이로 전체 순회. number_floors, mappa_index, start_after, number_floors_in_group 보존 |
| 층 데이터 | BALANCE/mappa_s.bin, SIR0 컨테이너 | [skytemple_files/dungeon_data/mappa_bin/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/dungeon_data/mappa_bin/handler.py) / MappaBinHandler; [skytemple_files/common/types/hybrid_data_handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/common/types/hybrid_data_handler.py)의 공통 deserialize | FileType.MAPPA_BIN.deserialize(bytes) → MappaBinProtocol.floor_lists. deserialize_raw를 SIR0 포함 데이터에 직접 호출하지 않음 |
| 층 레이아웃과 표 | 위 mappa_s.bin | [skytemple_files/dungeon_data/mappa_bin/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/dungeon_data/mappa_bin/protocol.py) / MappaFloorProtocol, MappaFloorLayoutProtocol | layout, monsters, traps, floor_items, shop_items, monster_house_items, buried_items, unk_items1/2 각각 저장 |
| 유효성 검사 | 추출 정의와 mappa | [skytemple_files/dungeon_data/mappa_bin/validator/validator.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/dungeon_data/mappa_bin/validator/validator.py) / DungeonValidator.validate | 그룹·범위·누락·중복 오류를 errors/invalid_dungeons로 보존. 도장 특례는 별도 검사 |
| 이름 | MESSAGE/{language.filename}, 예: text_e.str | [skytemple_files/data/str/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/data/str/handler.py) / StrHandler.deserialize; strings | config의 StringBlocks 시작 인덱스 + 던전 ID. 언어와 원본 문자열 인덱스를 함께 기록 |
| 진행 제한 | ARM9 DUNGEON_RESTRICTIONS | hardcoded/dungeons.py / HardcodedDungeons.get_dungeon_restrictions | 방향·아이템/인원/턴 제한 등을 원본 메타데이터로 보존. 전투·구출 규칙은 펫 행동에 이식하지 않음 |
| 고정층 | BALANCE/fixed.bin | FixedBin.fixed_floors, [skytemple/module/dungeon/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/module.py) / get_fixed_floor | layout.fixed_floor_id로 연결. 단순 던전 생성용 방 배치로 대체하더라도 원본 참조 기록 |

기본 정의는 이 판본 계열에서 180개 레코드이지만 문자열에 있는 256개 이름을 256개 플레이 가능한 던전으로 취급하면 안 된다. 코드에서는 설정 블록 길이에서 레코드 수를 계산하고 특수 ID를 따로 처리한다.

일반 던전의 로컬 층 k(0부터)는 floor_lists[d.mappa_index][d.start_after+k]에 연결한다. 단, SkyTemple의 get_mappa_floor(FloorViewInfo)는 이미 그룹 기준인 item.floor_id를 사용한다. 그 함수에 로컬 k를 그대로 넘겨 start_after를 누락하거나, 그룹 인덱스에 다시 start_after를 더하면 틀린 층이 된다. [skytemple/module/dungeon/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/module.py)의 get_mappa_floor, get_number_floors, _regenerate_dungeon_floors를 함께 참고한다.

| 도장 ID | 층 수 | mappa 그룹 0x35 내부 인덱스 |
| --- | --- | --- |
| 0xB4..0xBD | 각 5층 | k + (id-0xB4)*5 |
| 0xBE | 1층 | k+0x32 |
| 0xBF | 48층 | k+0x33 |

get_mappa_floor에는 0xBF..0xD3 분기도 있으나 이를 모두 독립적인 일반 던전으로 확장하지 않는다. 기본 배열에 없는 ID를 HardcodedDungeons 결과로 직접 조회하지 않는다. playable 여부, 0층 정의, 중복 그룹은 추출 manifest에 명시한다.

## 4. 포켓몬 출현과 원작 ID

[skytemple_files/dungeon_data/mappa_bin/_python_impl/monster.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/dungeon_data/mappa_bin/_python_impl/monster.py)의 MappaMonster는 8바이트 레코드를 읽는다. 저장 level은 u16 값을 512로 나눈 형태이며 main_spawn_weight, monster_house_spawn_weight, md_index는 별도 필드다. md_index=0은 종료/빈 항목 처리와 관계되므로 실제 종으로 내보내지 않는다. DUMMY_MD_INDEX=0x229와 케클레온 관련 행도 일반 출현 후보와 분리한다.

| 기능 | 실제 파일·API | 입력 → 출력 | 사용·연결·검증 |
| --- | --- | --- | --- |
| 기본 출현 종류 | MappaFloorProtocol.monsters | 층 → MD 인덱스 및 레벨 행 | normal과 monster_house 두 분포를 분리. 간단한 던전에서는 normal을 적용하고 MH 원본 표는 보존 |
| 누적값 해석 | [skytemple/module/dungeon/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/module.py) / calculate_relative_weights | 누적 정수 목록 → GCD로 축약한 상대 정수 가중치 | 0행은 0, 비영 행은 직전 비영 누적값과 차분. 음수·비단조·합계 0은 오류/빈 분포로 처리 |
| UI 검증 기준 | [skytemple/module/dungeon/widget/floor.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/widget/floor.py) / _init_monster_spawns | 층 출현 행 → 표시 확률 | 소스 확인; 실제 ROM과 표시 비교는 미실행 |
| 포켓몬 메타데이터 | BALANCE/monster.md; [skytemple_files/data/md/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/data/md/handler.py) / MdHandler.deserialize; [skytemple_files/data/md/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/data/md/protocol.py) / MdProtocol, MdEntryProtocol | MD bytes → 개체 레코드 | md_index, entid, national_pokedex_number, base_form_index, gender, signed sprite_index 저장 |
| 그림 참조 | [skytemple/core/sprite_provider.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/core/sprite_provider.py) / _retrieve_monster_sprite, _load_sprite_from_bin_pack | MD → sprite_index → BIN_PACK entry → WAN | GUI 미리보기는 첫 프레임일 수 있어 전체 애니메이션 추출에는 사용하지 않음 |

예: 누적값 [100,0,300,1000]의 차분은 [100,0,200,700], 상대 가중치는 [1,0,2,7]이다. [100,0,300,1000] 자체를 비례 추첨하면 원작 분포가 바뀐다. md_index_base=md_index % num_entities는 이름 관련 규칙이며 SpriteCollab 폼 경로를 만드는 규칙이 아니다. 이번 작성 시 고정 소스의 calculate_relative_weights 함수 본문을 AST로 분리해 GUI 초기화 없이 5개 입력(빈 목록·전부0·중간0·동일 누적값·단일 항목)을 실행했고 기대 결과를 통과했다. 공식 ROM 파서 실행과는 별개다.

신규 오프라인 변환 결과는 ROM 판본+md_index를 기본 키로 두고, 외부 그림 매핑은 별도 테이블로 관리한다. 서식·성별·별도 sprite_index를 합치지 않는다. signed sprite_index가 유효하지 않은 행은 missing_reference로 기록한다.

## 5. 아이템과 음식

| 데이터·기능 | 실제 위치·API | 입력 → 출력 | 변환과 연결 |
| --- | --- | --- | --- |
| 아이템 정의 | BALANCE/item_p.bin; [skytemple_files/data/item_p/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/data/item_p/handler.py) / ItemPHandler; [skytemple_files/data/item_p/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/data/item_p/protocol.py) / ItemPEntry | SIR0 bytes → 아이템 목록 | category, sprite, palette, item_id, is_valid 등을 보존. 레코드 인덱스와 item_id를 별도 저장 |
| 층별 아이템 표 | [skytemple_files/dungeon_data/mappa_bin/_python_impl/item_list.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/dungeon_data/mappa_bin/_python_impl/item_list.py) / MappaItemList.from_bytes | 압축된 목록 → categories/items 사전 | CMD_SKIP=0x7530 희소 인코딩, GUARANTEED=0xFFFF. 확률 수치로 해석할 값과 구분 |
| 분포 해석 | [skytemple/module/dungeon/widget/floor.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/dungeon/widget/floor.py) / _init_item_spawns, _split_items_in_list_in_cats | 카테고리·아이템 목록 → 상대 가중치 | 카테고리 CDF를 차분하고, 아이템은 카테고리로 나눈 뒤 guaranteed 행을 제외하고 각 CDF 차분 |
| 카테고리 정의 | Pmd2Data와 RomDataLoader로 보완된 item_categories | 아이템 ID → 카테고리 | XML 기본값만 고정하지 않음; 식품/베리 등은 실제 정의로 분류 |
| 그림 | DUNGEON/dungeon.bin의 논리 파일 items.itm.img; [skytemple_files/graphics/img_itm/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/img_itm/model.py) / ImgItm.to_pil(index,palette_index=0) | sprite/palette → PIL 이미지 | [skytemple/core/sprite_provider.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/core/sprite_provider.py) / _load_item__impl의 alpha 규칙을 재사용. 원래 팔레트 인덱스 px%16=0이 투명; RGB 검정을 모두 투명하게 만들지 않음 |

일반 아이템 확률은 P(category) × P(item|category)로 정리한다. floor/shop/monster_house/buried/unknown 목록은 서로 다른 문맥이며 합치지 않는다. GUARANTEED 항목은 별도 배열로 보존하고 일반 랜덤 추첨에 65535 가중치로 넣지 않는다. 이 표만으로 시간당 등장률이나 배치 개수를 알 수 없으며 layout의 밀도·원작 배치 알고리즘과 별개다.

확인 상수: MAX_ITEM_ID=363, MAX_CAT_IDS=0xF, POKE_ID=183. 전체 item_p 행 수를 363으로 자르지 않는다. 이 값은 mappa 인코딩 맥락의 범위이므로 item_p 전체 레코드와 구분한다. 공개 소스에서 round-trip assert가 존재하나 실제 ROM에 대한 통과 결과는 확보하지 못했다.

## 6. 타일셋·배경·계단

[skytemple_files/container/dungeon_bin/handler.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/container/dungeon_bin/handler.py)의 DungeonBinHandler.deserialize(data,static_data)는 BIN_PACK을 해석한다. [skytemple_files/container/dungeon_bin/model.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/container/dungeon_bin/model.py)의 DungeonBinPack.get/get_raw는 설정의 논리 이름에 연결한다. 원본 BIN_PACK 내부에 파일명이 있다고 가정하면 안 된다.

[skytemple_files/_resources/ppmdu_config/pmd2dungeondata.xml](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/_resources/ppmdu_config/pmd2dungeondata.xml)에서 확인한 0 기반 패키지 슬롯:

| 자산군 | t=0..169 또는 b=0..28 | 용도 |
| --- | --- | --- |
| 던전 DPLA/DMA/DPC/DPCI/DPL | t, 170+t, 340+t, 510+t, 680+t | 팔레트 애니메이션, 지형 자동 연결, 청크, 타일, 팔레트 |
| 배경 DPLA/DPC/DPCI/DBG/DPL | 850+b, 879+b, 908+b, 937+b, 966+b | 배경용 청크 지도; tileset 170 이상 계열 |

DMA는 SIR0/AT4PX, DPC/DPCI는 AT4PX, DBG는 SIR0/PKDPX 계열로 코드 경로가 구분된다. 컨테이너가 하위 파일을 자동 해석하는 경우에는 이미 해제된 결과에 다시 압축 해제를 적용하지 않는다.

| 기능 | 실제 소스 / API | 입력 → 출력 | 재사용 판단·비용 |
| --- | --- | --- | --- |
| 벽/바닥 연결 | [skytemple_files/graphics/dma/dma_drawer.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/dma/dma_drawer.py) / DmaDrawer.get_mappings_for_rules | 지형 그리드, variation_index?, treat_outside_as_wall → DPC 인덱스 그리드 | 새 2~3방 지도에 원작 DMA 규칙 적용; 오프라인 또는 층 변경 시만 계산 |
| 던전 그림 | 같은 파일 / draw(mappings,dpci,dpc,dpl,dpla) | 청크 참조 → PIL 프레임 목록 | 타일·팔레트 애니메이션 유지. 전체 지도 PNG 사전 저장은 종수·층수에 따라 용량 증가 |
| 청크 추출 | [skytemple_files/graphics/dpc/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/dpc/protocol.py) / chunks_to_pil, single_chunk_to_pil | DPC/DPCI/DPL → 청크 이미지 | 필요한 tileset만 atlas로 변환; 원작 팔레트/투명 인덱스 보존 |
| 생활 배경 | MAP_BG/bg_list.dat; [skytemple/module/map_bg/module.py](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/module/map_bg/module.py) / get_bma,get_bpc,get_bpl,get_bpas | 배경 목록 entry → MAP_BG/{lower-name}.bma/.bpc/.bpl/.bpa | BMA 지도+BPC 청크+BPL 팔레트+BPA 애니메이션을 묶어서 추출 |
| 배경 렌더 | [skytemple_files/graphics/bg_list_dat/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/bg_list_dat/protocol.py) / BgListEntryProtocol.get_*; [skytemple_files/graphics/bma/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/bma/protocol.py) / to_pil,to_pil_single_layer | 배경 묶음 → PIL 지도/레이어 | 이벤트에 필요한 생활 공간만 선택 |

DMA DmaType은 WALL=0, WATER=1, FLOOR=2이다. 이웃 비트는 S=0x01,SE=0x02,E=0x04,NE=0x08,N=0x10,NW=0x20,W=0x40,SW=0x80이다. [skytemple_files/graphics/dma/protocol.py](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/graphics/dma/protocol.py)의 DmaExtraType FLOOR1/WALL_OR_VOID/FLOOR2를 계단 그림으로 단정하지 않는다.

계단 위치는 원작 생성기의 출력이며 mappa에 미리 놓인 좌표가 아니다. 새 생성기가 계단 좌표를 만들고 원작 그림을 붙이는 구조로 분리한다. 계단 그래픽의 정확한 dungeon.bin 하위 슬롯·프레임은 미확인이다. 원작 계단 그림을 확보하기 전 임의의 타일을 원작으로 표기하지 않는다.

## 7. 자동 추출 호출 예시와 출력 계약

아래는 실제 파서 이름을 사용하는 최소 호출 예시다. 이 환경에서 실행하지 않았으며 전체 추출기가 아니다. 설치 환경과 ROM을 확보한 후 import·파싱부터 검증한다.

~~~python
from ndspy.rom import NintendoDSRom
from skytemple_files.common.util import get_ppmdu_config_for_rom, get_binary_from_rom
from skytemple_files.common.types.file_types import FileType
from skytemple_files.hardcoded.dungeons import HardcodedDungeons

rom = NintendoDSRom.fromFile('user_supplied.nds')
cfg = get_ppmdu_config_for_rom(rom)
arm9 = get_binary_from_rom(rom, cfg.bin_sections.arm9)
definitions = HardcodedDungeons.get_dungeon_list(arm9, cfg)
mappa = FileType.MAPPA_BIN.deserialize(rom.getFileByName('BALANCE/mappa_s.bin'))
md = FileType.MD.deserialize(rom.getFileByName('BALANCE/monster.md'))
items = FileType.ITEM_P.deserialize(rom.getFileByName('BALANCE/item_p.bin'))
pack = FileType.DUNGEON_BIN.deserialize(
    rom.getFileByName('DUNGEON/dungeon.bin'), static_data=cfg
)
# 이제 definitions + 도장 특례를 순회하고 각 floor_lists를 연결한다.
# 이 예시는 ROM.save 또는 set_*를 호출하지 않는다.
~~~

출력은 신규 schemaVersion=1 manifest + 정규화된 데이터 + 선택 자산으로 한다. JSON은 조사·검증용, 런타임은 크기 측정 뒤 경량 바이너리로 변환 가능하다. 새 파일명·스키마는 설계 제안이며 현재 저장소에 구현되어 있지 않다.

| 제안 출력 | 필수 필드 | 다음 단계 연결 |
| --- | --- | --- |
| manifest | ROM 해시, 판본/언어, 도구 커밋, 추출 범위, 파일 해시, 오류 | 모든 원본 키에 edition 포함 |
| dungeons | dungeonId, name/stringIndex, floorCount, mappaIndex,startAfter, playable, specialKind | Explorer가 dungeonId/localFloor로 floor key 조회 |
| floors | dungeonId,localFloor,groupFloor,layout 원본, spawnContext | SpawnSampler와 tileset 선택 |
| monsterSpawn | mdIndex,level,rawCDF,relativeWeight,normal/MH,specialKind | 전투 없이 우호적 개체 생성 |
| itemSpawn | context,categoryId,itemIndex/itemId,rawCDF,relativeWeight,guaranteed | 아이템 선택·획득·식사 분류 |
| pokemon/items | 원본 ID와 메타데이터, sprite/palette 참조 | Phase 2 asset manifest로 연결 |
| tilesets/backgrounds | packSlot/logicalName,sourceFiles,palette/frame 참조 | 새 지도와 생활 이벤트 렌더 |

자동으로 전체 정의·층·표를 순회할 수 있는 공개 파서 경로는 확보했다. 그러나 모든 ROM 판본·특수층·폼을 끝까지 추출할 수 있다는 실행 검증은 하지 못했다. 실패 항목을 건너뛰고 성공으로 표시하지 말고 별도 errors.json에 원본 키와 실패 원인을 저장한다.

## 8. 후속 검증과 인수인계

1. 동일 ROM으로 GUI의 던전 층·출현표와 추출 결과 대조: 일반 던전 첫/끝층, start_after가 있는 그룹, 0xB4/0xBE/0xBF 도장.
2. 모든 층 참조가 범위 안인지 검사. 정규화 CDF 비음수, 분포 합, guaranteed 분리, 미분류 카테고리 처리 확인.
3. 모든 mdIndex·item 참조·spriteIndex·palette·tileset이 실제 원본 범위 안인지 확인. 이름 중복은 허용하되 ID 충돌은 오류.
4. 원본 ROM 해시는 추출 전후 동일해야 한다. 대표 MAPPA/WAN/배경은 parse→serialize→parse 비교하고 그림 픽셀 검사를 별도로 수행한다.
5. Phase 2는 이 파일의 ID·그림 참조를 재사용한다. Phase 4는 원본 분포와 배치 개수·지형 생성 정책을 구분한다.

미확인: 실제 ROM 전체 추출, 지역별 전체 지원, 계단 그림 슬롯, 일부 특수항목 의미, 모든 생활 공간 자산. 메모리·설치 크기는 오프라인 추출 도구와 Windows 런타임을 따로 측정해야 한다.
