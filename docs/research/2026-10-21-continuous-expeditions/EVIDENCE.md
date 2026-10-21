# 원작 연결부 조사 근거

보존 ROM은 `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`다. C2SE / EoS_NA이며 패치된 한국어 문자열은 해독하지 않았다. `latin-1`은 대사 번역이 아닌 명령어 구조 확인에만 사용했다.

- [추출 결과](extraction.json): SSB 3,760개, SSA/SSE/SSS 1,884개 전체 파싱. 파싱 예외 0개이며 알 수 없는 opcode 인수에 대한 라이브러리 경고는 그대로 남았다. 모든 인수 의미를 해명했다는 뜻이 아니다.
- [스크립트 해시 목록](script-manifest.csv): 원작 경로·바이트 수·SHA256.
- [동상 배치](statue-layouts.json): 전체 layout에서 actor의 `entid=554`를 검색한 60개 배치 파일. 진입 `enter.sse`는 57개 맵이다.
- [ground 목록](grounds.json): SkyTemple 정적 level ID, 실제 ROM 경로·배치·배경 파일 해시, 저장·창고 상호작용, 타일 연결.
- [명령어 교차 참조](routing-evidence.json): 선택 ground의 이야기 스크립트·진입·동상 명령어와 공통 스크립트. 대사 본문은 제외했다. 조건 분기 전체를 실행한 동적 경로 추적이 아니므로 이 목록의 `main_EnterDungeon`을 전부 무조건 연결해서는 안 된다.
- [타일 연결 테이블](ground-tileset-mappings.json): ROM overlay11에서 읽었다. `floorId=100`은 실제 100층이 아니다. 원작/라이브러리의 clamp 규칙으로 대상 던전 마지막 층에 대응한다. [grounds.json](grounds.json)의 `tilesetMapping`에 해석 결과를 별도로 보존했다.
- [웹 헤더 비교](web-rest-stops.json): 29개 페이지의 짧은 rest-stop 헤더. `None`을 입구 동상 부재나 다른 에피소드의 동상 부재로 해석하지 않았다.

동상은 종 번호115를 검색해서 찾은 것이 아니다. SSA actor155 `OBJECT001` / actor156 `OBJECT002`의 `entid554`가 가리키는 MD554, sprite597, `MONSTER/m_ground.bin`을 직접 그려 캥카 동상임을 확인했다. 해당 actor의 `scriptId`에 대응하는 `enterNN.ssb`가 `CORO_HANYOU_SAVE_FUNC`를 호출하는지 검사했다. 57개 모두 확인했으며 이 가운데33개는 `MENU_STORAGE`도 직접 호출한다. 공통 저장 함수는 `MENU_SAVE_MENU`를 사용한다.

조사 맵77개에는 동상 진입 맵57개 외에 돌 퍼즐·보스 전환·스토리 연출을 구별하기 위한20개가 포함된다. 동상 배우가 검출되지 않은 맵은 `not_detected_by_actor_scan`으로 남겼다. 별도 오브젝트나 엔진 저장 기능까지 부재하다고 단정하지 않는다.

## 타일과 표본 이미지

`mapType=10/11`인 일부 ground는 MAP_BG의 기본 그림만 읽으면 노란 임시 통로로 보인다. 실제 표면은 overlay11의 ground→dungeon/floor 연결, MAPPA의 tileset, `DUNGEON/dungeon.bin`의 DMA/DPC/DPCI/DPL로 결정된다. 원본 BMA 지형 규칙에 이 타일을 연결해야 한다.

[표본 생성 근거](preview-evidence.json), [시한의 탑 중간 쉼터 표본](temporal-tower-midpoint.png), [동상 프레임](kangaskhan-statue.png).

전체 표본: [1](connectors-1.png) · [2](connectors-2.png) · [3](connectors-3.png) · [4](connectors-4.png).

표본은 정적 팔레트·타일 변형0·원작 동상 배우 한 프레임만 합성한 **조사용 표본**이다. 원작 실행 화면, 다른 등장인물/물체, 팔레트 애니메이션과 이벤트 재생을 재현한 것은 아니다. 특히 Aegis의 돌 퍼즐 오브젝트는 이 표본에 합성하지 않았다.

## 재현

저장소 루트에서 순서대로 실행한다. 새 라이브러리를 설치하지 않고 기존 pinned 환경(SkyTemple Files1.8.5)을 사용한다.

```bash
local/p1-venv/bin/python docs/research/2026-10-21-continuous-expeditions/audit_connectors.py
local/p1-venv/bin/python docs/research/2026-10-21-continuous-expeditions/preview_connectors.py
local/p1-venv/bin/python docs/research/2026-10-21-continuous-expeditions/classify_routes.py
```

라이브러리의 [ground 타일 해석](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/hardcoded/ground_dungeon_tilesets.py)과 SkyTemple의 [ground 표시 구현](https://github.com/SkyTemple/skytemple/blob/f78d0ef387dbc121bad4c8f0186237f9a4ae035d/skytemple/core/mapbg_util/map_tileset_overlay.py)을 대조했다. 기존 [전체 맵 조사](../2026-10-21-full-map-audit/README.md)의 원작 숫자 ID와 MAPPA 정의를 재사용했다.
