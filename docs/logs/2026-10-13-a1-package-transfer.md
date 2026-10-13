# A-1 원작 패키지 세션 간 전달

2026-10-13 KST. 사용자가 다른 세션에서 A-3 진행 중 원작 패키지가 없다고 보고했다.

## 원인

- pipeline §2에 따라 원본 ROM·추출 게임 자산은 Git에 커밋하지 않았다.
- A-1 패키지는 이 작업 환경의 `local/a1-rom-final-01`에 보존돼 있다. Git clone과 별도 실행 환경에는 자동 복제되지 않는다.
- 인수인계의 로컬 경로 안내만으로 별도 세션에 실제 파일이 전달되는 것은 아니다. 전송 산출물을 별도로 제공했다.

## 수행·검증

- 기존 원작 패키지 `python -m tools.mdpet validate local/a1-rom-final-01` 재검사 pass(2 assets/348 frames).
- `local/transfer/a1-original-package.zip` 생성: 587,906bytes, 219 files. ZIP CRC 전체 검사 pass.
- ZIP SHA-256: `834786ad283bc5980333867988192522891ca3323a996b05b6c6a1ee2ae27bb4`.
- manifest SHA-256: `c657d24ce3da04c8c20a289c9a2f5ee493838a9ab2471ad31723406b7b294c8d`.
- root directory `a1-rom-final-01/`. manifest/catalogue/PNG atlas/지원·오류·검증·시트 감사 파일 포함. 원본 ROM 제외.
- 전송 요약 `local/transfer/a1-original-package-transfer.json`에 크기/해시/범위 기록. ZIP와 자산은 계속 Git 제외.

## A-3 세션 재개

사용자가 ZIP을 해당 세션에 첨부하거나 두 세션이 동일 파일시스템이면 기존 경로를 사용한다. ZIP을 해제한 `a1-rom-final-01`에 A-2의 실제 패키지 변환 도구를 적용한다. A-1은 JSON/PNG이고 A-2 런타임 형식과 다르면 그대로 런타임 로더에 넘기지 않는다.
이 패키지는 필수 동작 지원 원작 2종의 대표 범위다. 한국어 문자 매핑/전체 종·폼/원작 wall time/Windows gate는 기존 미검증 상태를 유지한다.
