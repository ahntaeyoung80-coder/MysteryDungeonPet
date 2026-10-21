# 맵별 적·아이템·진행 자료 읽는 법

이 자료는 기존 전체 맵 조사와 연속 원정 분류를 보강한다. 원작 정의192개 각각의 상세 문서는 [기존 ID 색인](DUNGEON_ID_INDEX.md)에서 연다. 현재 게임의15·18 패키지나 귀환 규칙을 변경한 결과가 아니다.

## ID와 자료 구조

| 자료 | 내용 |
|---|---|
| [map-gameplay.json](map-gameplay.json) | SEG ID192개, 실제 층 참조, 입장 제약 원본, 진행/막간 ID와 상세 문서 경로 |
| `maps/SEG-Dxxx.md` | 맵별 적/레벨/출현 층, 아이템/문맥/선택 비율, 고정 배치, 진행 조건, 층별 설정 |
| [map-gameplay-details.jsonl.gz](map-gameplay-details.jsonl.gz) | 맵별 Markdown의 전체 요약 행과 층 설정 원본 |
| [gameplay-floor-profiles.jsonl.gz](gameplay-floor-profiles.jsonl.gz) | 물리 MAPPA1,892층의 raw CDF·6개 아이템 문맥·함정·layout |
| [enemy-floor-rates.csv.gz](enemy-floor-rates.csv.gz) | 구간/층/MD/레벨/일반·MH 가중치·분모·raw CDF |
| [item-floor-rates.csv.gz](item-floor-rates.csv.gz) | 구간/층/아이템 record/문맥/종류와 아이템 가중치·분모·선택 비율 |
| [gameplay-fixed-spawns.json](gameplay-fixed-spawns.json) | 고정층/대체층112개 배치의 좌표, 적/동료/아이템 구분, 원작 전용 능력치 |
| [gameplay-monsters.json](gameplay-monsters.json) | MD 식별명, 도감 번호, 성별/폼/스프라이트, 타입/특성 ID·추가 출현 조건 플래그 |
| [gameplay-items.json](gameplay-items.json) |1,400개 원본 record ID와 itemId·이름·분류·그림·가격·기술 ID |
| [gameplay-extraction.json](gameplay-extraction.json) | 추출 건수·ROM/binary hash·미해결 참조·기존 profile 대조 |

`FLR-G026-F000`의 G는 MAPPA 그룹, F는 그룹 안0기반 층이다. `SEG-D041`은 원작 dungeon ID41이며 서로 다른 단위다. 문서의 ‘층’은 해당 SEG 안1기반 순서이고 게임의 B/F 표시나 원정 전체 누적 층수가 아니다. 여러 던전이 같은 물리 층을 공유하므로 logical 층과 physical 층을 합산해 새 맵 수로 세지 않는다.

## 적 표 해석

- 일반 출현과 몬스터하우스 후보를 분리한다. 표의 비율은 해당 문맥의 유효 후보 안 상대 가중치이며, 한 턴·한 층의 실제 조우 확률이 아니다. 켈리몬/종료·더미 행은 원본 profile의 `specialRows`에 별도로 있다.
- MD ID는 도감 번호가 아니다. 같은 종의 성별/폼도 다른 MD로 보존한다. 영문 이름은 고정 [pmdsky-debug enum](https://github.com/UsernameFodder/pmdsky-debug/blob/c72fe675919ec83cfe93f687973afae85e00d1a3/headers/types/common/enums.h)이며 패치 ROM의 한국어 명칭을 해독했다는 뜻이 아니다.
- 고정층의 MAPPA 후보는 실제 고정 전투 적 명단과 다를 수 있다. 문서에서 `고정층 원본 후보`로 표시하고, 별도 고정 배치 표를 우선 확인한다. 적대/우호 구분은 fixed spawn type 원본을 사용한다. 일반 후보를 이 프로젝트의 우호 NPC로 바꾸는1/4 규칙은 원작 적 표에 적용하지 않는다.
- MD의 추가 출현 조건 플래그, 스토리/도구/임무 조건은 별도로 적용될 수 있다. 후보 이름이 있다는 사실만으로 항상 출현한다고 표시하지 않는다.

## 아이템 표 해석

원작 선택은 문맥별 **종류 선택 → 해당 종류 안에서 아이템 선택**의 두 단계다. 표의 선택 비율은 `종류 가중치/종류 합계 × 아이템 가중치/해당 종류 합계`다. Markdown의 최소~최대는 표에 적힌 층들 사이의 범위이며, 층별 정확한 분수와 raw CDF는 CSV/profile에 있다. 단순히 아이템 후보를 같은 확률로 섞지 않는다.

바닥·상점·MH·매장 아이템과 의미 미확정 문맥 `unk_items1/unk_items2`를 구별한다. 상점 후보가 있어도 layout의 상점 발생이0이면 실제 상점 존재를 보장하지 않는다. MH도 같은 원칙이다. 밀도 수치는 엔진 입력이며 실제 생성 개수를 뜻하지 않는다.

`rawCDF=65535`의 확정 표시는 무작위 선택 비중과 분리했다. 이것만으로 매 층 해당 아이템을 반드시 주는 제품 규칙을 만들지 않는다. 고정 배치 아이템, 안농 문자돌 같은 특수 처치 드롭, 스토리/보물 보상도 일반 바닥 표와 구별한다.

MAPPA 아이템 키는 `item_p`의 **recordIndex**다. 고정층 item spawn은 **itemId**를 참조한다. 두 값을 이름이 같다는 이유로 합치지 않는다. 정적 이름의 표시용 제어 태그는 Markdown에서 제거하지만 원본 이름은 JSON/CSV에 보존한다.

## 진행·연결·제약

각 상세 문서에 기존 RUN/ARC, 들어오는/나가는 LNK, 캥카 ground와 저장/창고 기능, 반복/재방문 조건을 붙였다. 원정 마지막 성공과 중간 SEG 성공을 구별하며, 후속 연결이 아직 없다는 사실을 자동 길드 복귀의 근거로 쓰지 않는다.

입장 제한과 layout 수치는 보존 ROM의 원본 값이다. 스토리 멤버 고정·상태별 해금·스크립트 덮어쓰기까지 적용한 최종 엔진 결과가 아니다. 본편과 특별 에피소드의 동명 던전도 ID를 유지한다.175~178의 ground 별명, 테스트/더미/구조/도장을 독립 일반 탐험으로 간주하지 않는다.71/173의6개 범위 밖 참조는 대체하지 않았다.

고정층 대체 테이블은 원본 base/override를 함께 보존한다. 적용하는 스토리/재방문 조건을 실행 검증한 결과로 표시하지 않는다. 예를 들어 시한의 탑 정상의 fixed10에는 PRIMAL_DIALGA MD552, 대체 fixed74에는 DIALGA MD525가 각각 있다.

## 재현

```bash
local/p1-venv/bin/python docs/research/2026-10-21-full-map-audit/enrich_gameplay.py
local/p1-venv/bin/python docs/research/2026-10-21-full-map-audit/write_gameplay_docs.py
local/p1-venv/bin/python docs/research/2026-10-21-full-map-audit/validate_gameplay.py
```

[고정층 파서 원본](https://github.com/SkyTemple/skytemple-files/blob/5ed359a2560a1f04666b9ea02b71649b89cb16d7/skytemple_files/hardcoded/fixed_floor.py), [연속 원정 분류](../2026-10-21-continuous-expeditions/README.md), [최종 검증](gameplay-validation.json).
