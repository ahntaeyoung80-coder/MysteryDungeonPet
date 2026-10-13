# Windows A-3 원작 패키지 실행

2026-10-13 KST. 이 설치물은 이전 A-2 시험 빌드와 다르다. 선택 원작2개 자산을 포함하며 기본 EXE는 MD1/sprite0을 재생한다. 원본 ROM·전체 내부 파일·Python·개발 서버는 필요 없다.

## 실행

1. ZIP 전체를 해제하고 `MysteryDungeonPet-A3` 또는 GitHub artifact의 `package` 폴더를 연다.
2. `MysteryDungeonPet.exe`를 실행한다. `assets`·`licenses`·스크립트/안내 파일을 EXE와 함께 둔다.
3. 몸체 또는 트레이 아이콘을 우클릭하면 동작·방향·Loop/Finite·Stop·배율·topmost·Hide·Exit를 선택할 수 있다. 창은200×200 실제px 고정이며 카메라 조작 메뉴는 없다.
4. Hide 후 트레이 아이콘을 더블클릭해 복원한다. Sleep은 Stop 또는 다른 동작으로 종료한다.

두 번째 원작 개체와 Pose를 선택하는 사용 예:

```powershell
.\MysteryDungeonPet.exe --package assets\original\asset-1\asset.mdpa --clip 17 --direction 0 --finite
```

검사용 합성 fixture를 명시적으로 선택하려면 `--package assets\fixture\fixture.mdpa`를 사용한다. 동작/방향마다 실제 지원 행을 사용하며1행 Eat/Sleep을 가짜8방향으로 늘리지 않는다. 자율 탐험·자동 생활 기능은 아직 없다.

## 기록과 판정

기본 기록은 `%LOCALAPPDATA%\MysteryDungeonPet\metrics.csv`와 `events.log`다. CSV에는 동작/방향/프레임/장면좌표와 마지막 유효 창 정보가 남는다. EXE는 애니메이션·Windows 런타임으로 실행하며 Python은 분석 도구를 사용하는 개발자에게만 필요하다.

기존100px 저장 설정은 위치·배율·topmost만 복원하고 창 크기는200px로 유지한다. 원본 자산을 찾지 못하면 ZIP 전체를 해제했는지, EXE 옆 `assets\original\asset-0\asset.mdpa`와 `atlas.png`가 있는지 확인한다.

이번 MSVC Release 빌드와 원작2개 WIC/cache·native tick 재생·단기 CSV 검사15개는 [Windows CI](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38358343599)에서 통과했다. CI 로그에서는 실제 원작 프레임 진전, 종료 DPI96/200px 유지와 empty의 애니메이션 중단을 확인했다. 원작 wall time·실제 사용자Win+L/display-off/절전·DPI변경·장기 성능의 완료 판정은 별도다.

이 안내는 실행 방법이며 추가 수동 테스트 요청이 아니다. 기존 사용자Windows11 결과를 재사용한다. 상세 개발 검사와 다음 단계는 [A3_USAGE.md](A3_USAGE.md)를 따른다.
