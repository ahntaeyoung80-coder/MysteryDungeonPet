# 원작 입력 전체와 검증 패키지

2026-10-13 사용자가 원본 ROM까지 포함한 입력 전체를 private 저장소에 보관하도록 명시했다. 다른 세션은 `work`를 최신으로 받아 이 경로를 사용한다. 원본 파일은 읽기 전용 입력으로 취급하며 인증 정보는 포함하지 않는다.

## 바로 사용할 입력

| 경로 | 내용 |
| --- | --- |
| `source/user-supplied.nds` | 사용자 제공 원본 ROM 전체. 재추출에 필요한 header/table/파일을 포함한 정확한 복사본 |
| `source/rom-files.zip` | 원본 내부 모든 파일과 ARM9/ARM7/overlay tables/post data를 해제한 압축본 |
| `source/inventory.json` | 모든 파일 ID·원본 경로·크기·SHA-256과 압축본 해시·검사 결과 |
| `a1-original-package.zip` | 이전 A-1 실제 원작 검증 산출물 전체. root=`a1-rom-final-01/` |
| `runtime/asset-0/asset.mdpa` | A-2/A-3에서 바로 읽을 MD1/sprite0 패키지 |
| `runtime/asset-1/asset.mdpa` | A-2/A-3에서 바로 읽을 MD4/sprite4 패키지 |
| `runtime-consumer-validation.json` | 원작 2종의 C++ 소비 계약 대조 결과 |

**전체 입력 보관과 전체 변환 지원은 별개다.** 모든 원본 파일은 보관하지만, 현재 변환·프레임 검증 범위는 이전 A-1의 대표 2종이다. 모든 종/폼/동작·타일/배경의 PNG/MDPA 변환이나 재생을 완료했다고 표시하지 않는다. 전체 원본이 있으므로 이후 세션은 사용자에게 재업로드를 요청하지 않고 필요한 자산을 추출할 수 있다.

## 원본 재추출

```bash
.venv/bin/python -m tools.mdpet extract --rom assets/original/source/user-supplied.nds --out local/new-rom-extraction --allow-undecoded-text
```

기존 추출기는 대표 1~2개 선정용이다. 전체 종/폼 변환을 진행할 때 반복/지원 감사 경로를 확장하고 누락·오류를 기록한다. `text_e.str` 문자 매핑은 아직 unresolved이며 rawNameHex를 보존한다.

## A-3 원작 재생 입력

```powershell
.\MysteryDungeonPet.exe --package assets\original\runtime\asset-0\asset.mdpa --clip 17 --direction 0 --finite
```

실행 파일 위치에 맞춰 package 경로를 바꾼다. Windows 실제 실행·원작 wall time·전체 지원은 별도 검증 항목이다. 새 변환에 필요한 A-1 감사본은 ZIP을 해제해 `tools/compile_runtime_package.py --package 해제경로/a1-rom-final-01 --out 새경로`에 입력한다.

출처와 해시: `transfer.json`, `source/inventory.json`, 각 runtime의 `CREDITS.txt`/provenance. 기본 배포 크기와 원본 입력 저장소 크기는 구분한다. 전체 ROM을 실행 프로그램의 설치물에 자동 포함하지 않는다.
