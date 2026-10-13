> 2026-10-13 최신 실행 범위: ZIP 전체 해제 후 `MysteryDungeonPet.exe`를 더블클릭하면 자동 탐험·기본공격 전투·길드 공동 식사·침실 수면·기상·다음날 탐험을 시작한다. `Run-B1.ps1`도 호환 유지한다. 방향·애니메이션 검사에는 `--diagnostics`를 명시한다. 현재 실행 안내는 [WINDOWS_QUICKSTART.md](WINDOWS_QUICKSTART.md), 완료·미완료 항목은 [B1_GAMEPLAY_TASK.md](B1_GAMEPLAY_TASK.md)를 따른다. 아래 본문은 B-1 독립 코어의 과거 비전투 체크포인트다.

# B-1 비전투 탐험 코어와 원작 타일

2026-10-13 KST. 최신 `origin/work`의 기준 커밋 `19ea3a1e34b9d157c9a04fef460ddff5ede324b3`에서 A의 Extractor/정규화·MDPA·Animator·WIC·Surface·200px 창을 재사용했다. 사용자 최신 지시로 A-3 잔여 검증을 pending으로 유지하면서 B-1 독립 작업을 진행했다. 전투는 구현하지 않았다.

## 구현 범위

- `src/core/exploration.*`: 24×24, 겹치지 않는 슬롯의 2~3방, 중심 간 L통로, 4방향 FIFO BFS, 다른 방의 가장 먼 중심에 계단. 지도와 논리 좌표는 Windows 화면 좌표와 분리한다.
- 아이템 → 우호적 친구 → 미방문 타일 → 계단 순으로 **도달 가능한** 목표를 선택한다. 지도 전체를 알고 있는 첫 구현이며 시야/FOV는 추가하지 않았다. 친구는 `encounterPolicy`의 Greet를 사용한다. HP·공격·피해 필드가 없다.
- 경로는 목표/지도/점유 변경 때 재계획한다. `setBlocked`가 점유 버전을 올리고 캐시를 무효화한다. 막힌 개체를 선택하지 않으며 대안 목표·계단으로 진행한다. 층당 4,608 action 뒤 계단을 우선하고 추가 576 action을 넘거나 출구가 없으면 미클리어 안전 귀가한다. 친구는 기본적으로 통행을 막지 않으며 blocker 검사는 별도 점유 마스크를 사용한다.
- 배치는 도달 가능한 비점유 바닥의 비복원 추첨이다. guaranteed 우선, 일반 아이템 요청3/예산8, 친구 요청2/예산4. 예산에는 guaranteed도 포함한다. 초과·공간 소진·빈 분포는 집계된 사유를 남긴다. 수십억 요청도 요청 횟수만큼 재시도하거나 로그를 만들지 않는다.
- 아이템 ID는 `item_p recordIndex`, 친구 ID는 MD index다. 원작 normal 상대 가중치와 `floor_items`의 category/conditional 가중치를 그대로 사용하며 원본 개수·시간당 출현율의 재현을 주장하지 않는다. 원본 category guaranteed 정책이 미확정이면 변환기를 명시적으로 실패시킨다. 일반 guaranteed item 배열은 지원한다.
- `runId:floorIndex:instanceId`를 포함한 `pickup/greet/stairs/clear` onceKey와 인벤토리·제거 결과를 같은 snapshot에 저장한다. 마지막 계단에서 clear를 기록하고 Pose17을 요청하며, finite 종료 또는 8 action timeout 뒤 Home으로 간다. C의 자동 재출발·생활 루프는 구현 범위에 없다.

## 원작 입력과 변환

기존 catalogue는 `assets/original/a1-original-package.zip`의 `a1-rom-final-01/catalogue.json`이다. A의 `build_floor`를 재사용해 선택 던전의 모든 층을 읽으며 겹치는 기존 profile은 의미 데이터가 같은지 대조한다. 대표 dungeon0은 3층/tileset0, dungeon15는 localFloor0..6/groupFloor8..14/tileset12다. 서로 다른 던전의 대표 층을 한 던전처럼 이어 붙이지 않는다.

```bash
.venv/bin/python -m tools.mdpet.b1 \
  --rom assets/original/source/user-supplied.nds \
  --catalogue-zip assets/original/a1-original-package.zip \
  --dungeon 0 --out local/b1-new-dungeon0
```

`requirements-offline.txt`의 기존 고정 의존성과 `smoke()`를 사용한다. 출력은 새 디렉터리로만 확정하며 기존 정상 출력을 덮어쓰지 않는다. 원본 ROM 전후 SHA-256을 검사한다. 매 층 Python을 실행하지 않는다.

원작 DUNGEON/dungeon.bin의 DMA/DPC/DPCI/DPL을 공식 파서로 해석한다. DPC 400개 청크(24×24)를 480×480 atlas와 **기존 MDPA**에 담고, DMA의 3 terrain×256 neighbors×3 variation=2,304개 규칙을 MDPE에 보존한다. runtime의 두 지형은 wall=0/floor=1이며 DMA wall=0/floor=2로 명시 매핑한다. 이웃 비트는 S/SE/E/NE/N/NW/W/SW, 바깥은 wall, 현재 표시 variation=0이다. 색상이 검정인지와 무관하게 원본 팔레트 index%16=0만 투명으로 처리한다.

배포용 선택 출력은 `assets/original/exploration/dungeon-0/`, `dungeon-15/`에 있다. 변환 감사용 catalogue/JSON과 원본 ROM은 런타임 설치에 포함하지 않는다. 2개 던전의 실행 파일 외 추가 데이터 총량은153,968bytes다. 모든 던전·종/폼 지원을 뜻하지 않는다.

## 빌드·검증·실행

```bash
cmake -S . -B build/b1 -DCMAKE_BUILD_TYPE=Release
cmake --build build/b1
ctest --test-dir build/b1 --output-on-failure
PET_PACKAGE_INSPECT="$PWD/build/b1/pet_package_inspect" \
  PET_EXPLORE="$PWD/build/b1/pet_explore" .venv/bin/python -m unittest discover -s tests -v
python docs/research/checks/geometry_check.py
build/b1/pet_exploration_tests \
  assets/original/exploration/dungeon-0/exploration.mdpe \
  assets/original/exploration/dungeon-15/exploration.mdpe
build/b1/pet_explore assets/original/exploration/dungeon-0/exploration.mdpe 42 local/b1-run.mdes
```

마지막 명령을 다시 실행하면 저장된 run을 복원한다. Home이면 획득·클리어를 재실행하지 않는다.

Windows 설치물은 더블클릭으로 탐험한다. 아래 명시적 탐험 명령도 호환 유지한다. 기존 A 재생·계측 옵션은 명시적으로 선택한 진단 모드에서 유지한다.

```powershell
.\MysteryDungeonPet.exe --explore assets\exploration\dungeon-0\exploration.mdpe --seed 42
```

기본 저장 위치는 `%LOCALAPPDATA%\MysteryDungeonPet\exploration.mdes`다. `--snapshot 경로`로 별도 저장 위치를 지정하고 `--new-run`으로 해당 위치에 새 run을 시작한다. 다른 던전/변환 버전의 snapshot은 임의 해석하지 않고 assetVersion 불일치로 거부한다. 던전15를 시작하려면 다른 snapshot 경로를 사용한다.

UI thread는 350ms action deadline에 한 번 진행한다. 지도 이동은 창 내부 카메라로 표시하고 창의 화면 위치를 바꾸지 않는다. 원작 타일과 몸체 atlas는 최초 로드 후 캐시하며 표시 영역과 교차하는 레이어만 합성한다. Hide/lock/display/suspend 중 탐험도 멈추고 복귀 시 과거 action을 폭주 재생하지 않는다. 메뉴의 수동 동작/방향 조작은 탐험 중 표시하지 않는다. Windows 탐험 연결 검사기는 `tests/exploration_windows_tests.ps1`이며 CI의 기존 Windows 검사와 함께 실행한다. run38367994286의Windows Server2025/MSVC Release에서UI이동/타일WIC·decode2회/창고정/저장복원 검사가pass했다. 사용자Windows11·실제OS사건 검증과구분한다.

## snapshot 계약과 중단 복원

`MDES` schema1 little-endian, 최대8MiB, CRC32. assetVersion·runId·원작 FloorKey/프로필·mapSeed·현재 RNG·지도·점유/방문·entity/instanceId·목표·deadline counter·Explore/Pose/Home·inventory·onceKeys를 보존한다. 경로 포인터/OS handle/애니메이션 프레임은 저장하지 않는다. 복원 시 저장된 목표로 BFS 경로를 재구축하므로 같은 RNG와 최종 결과를 유지한다.

생성기/추첨 RNG는 SplitMix64+편향 제거 bounded draw이며 원작 게임 RNG와 별개다. 기존 Python geometry의 LCG와도 다르므로 두 결과의 최대 타일·거리 값이 같아야 하는 것은 아니다.

획득·인사·층 이동·clear는 candidate에 처리 → 검증·임시 파일 write/flush → atomic replace → candidate 공개 순서다. Windows는 FlushFileBuffers/MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH), POSIX는 fsync/rename/부모 directory fsync를 사용한다. `.tmp`는 복원 입력으로 사용하지 않는다. 마지막 정상 commit 전 종료되면 이전 snapshot부터 계속한다. onceKey는 저장된 게임 효과의 중복을 막는다. 소리/렌더가 종료 직전에 실제 표시됐는지까지 보장하는 외부 부작용 프로토콜은 아니다. 실제 정전·스토리지 장애 내구성은 별도 미검증이다.

## 판정과 남은 항목

기존 Python geometry와 native10,000 seed 결과는 각각 `docs/validation/2026-10-13-b1-python-geometry.json`, `2026-10-13-b1-native.json`에 보존한다. Python 원본 검사 파일과 과거 조사 JSON은 수정하지 않았다. 전체 증거·환경·명령은 B1 summary와 로그를 따른다.

현재 요청한 지도·BFS·목표·once·저장/복원 코어와 선택 원작 타일 변환을 구현했다. 원작 아이템/친구 그림은 아직 연결하지 않아 아이템 노랑/친구 파랑/계단 회색 **검증 표시**를 쓴다. 인사 후 친구 표시를 제거하며 반복 인사 cooldown·생활 연동은 후속 범위다. 원작 계단 그림 매핑은 unresolved, DPLA 팔레트 애니메이션·이동 보간·원작 친구 연출·전체 던전 지원과 Windows11 탐험 육안/장기 성능은 pending이다. 이 제한이 남아 있으므로 B-1 제품 전체 완료로 판정하지 않는다. A-3의 원작 wall time/anchor/shadow·실제 OS 사건 잔여 검증도 pending을 유지한다. B-2는 미착수다.
