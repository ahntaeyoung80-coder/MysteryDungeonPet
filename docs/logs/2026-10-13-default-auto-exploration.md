# EXE 기본 실행의 자동 탐험 오류 수정

2026-10-13 사용자 ZIP 실행 보고로 착수. 원인: `Options.exploration` 기본값이 빈 경로여서 인자 없는 EXE는 A-3 Walk0 수동 진단을 실행했고 방향 메뉴도 조작 가능했다. 기존 Windows 검사는 모두 `--explore`를 지정하여 이 제품 진입 경로를 검사하지 못했다.

수정: 기본 실행은 EXE 기준 dungeon0 경로와 기존 gameplay 자산을 사용한다. 진단은 명시적 `--diagnostics` 또는 기존 A selector로 유지한다. 탐험에서는 수동 동작·방향·Loop/Finite/Stop·시험 배경·Reload 메뉴를 제거한다. Run-A2는 진단을 명시하고 기본 설치 안내는 자동 탐험으로 교체한다. 기존 저장을 삭제하지 않고 복원한다.

검증 계획: Linux 기존4CTests, Windows 크로스 빌드, MSVC 기존 전투·길드 생활 회귀, 설치된 EXE 무인자 실행·다른 CWD·실제 메뉴·저장 복원. 새 ZIP은 실제 Windows 검증이 통과한 설치물을 사용한다. 실행 결과 수집 전 pending.
