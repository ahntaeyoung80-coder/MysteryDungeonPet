# EXE 기본 실행의 자동 탐험 오류 수정

2026-10-13 사용자 ZIP 실행 보고로 착수. 원인: `Options.exploration` 기본값이 빈 경로여서 인자 없는 EXE는 A-3 Walk0 수동 진단을 실행했고 방향 메뉴도 조작 가능했다. 기존 Windows 검사는 모두 `--explore`를 지정하여 이 제품 진입 경로를 검사하지 못했다.

수정: 기본 실행은 EXE 기준 dungeon0 경로와 기존 gameplay 자산을 사용한다. 진단은 명시적 `--diagnostics` 또는 기존 A selector로 유지한다. 탐험에서는 수동 동작·방향·Loop/Finite/Stop·시험 배경·Reload 메뉴를 제거한다. Run-A2는 진단을 명시하고 기본 설치 안내는 자동 탐험으로 교체한다. 기존 저장을 삭제하지 않고 복원한다.

검증 계획: Linux 기존4CTests, Windows 크로스 빌드, MSVC 기존 전투·길드 생활 회귀, 설치된 EXE 무인자 실행·다른 CWD·실제 메뉴·저장 복원. 새 ZIP은 실제 Windows 검증이 통과한 설치물을 사용한다. 실행 결과 수집 전 pending.

로컬 실행 결과: Linux Release 기존4CTests pass(17.40초), MinGW x64 Windows compile/link pass. 런타임 커밋8decf4f6122d11b12f829db8097e9ab179a10dda를 원격 fix/b1-life-combat에 push했다. Windows MSVC 실제 설치물 검사는 CI38411109815에서 진행 중이다.

최종 결과: Windows Server2025/MSVC CI38411109815 core/windows 모두success. ASan/UBSan4CTests와Python23tests, MSVC4CTests·기존 WIC/재생·전투/길드 생활 GUI 검사가 통과했다. 설치된 EXE를 인자 없이 다른 CWD에서 실행했고 최초 실행은 자동 탐험에서 즉시 플레이어 선공 전투로 진행했다. 종료 후 인자 없이 재실행하면 동일 runId364169234의 Faint 상태를 복원해 승리→원래 탐험을 재개했다. 두 번째 실행의 지도 좌표도4개로 진전했다. 실제 메뉴는 Hide/Always on top/Scale1..3/Exit이며 Animation/Direction/Loop/Finite/Stop 등이 없다. --diagnostics 실행은 decode1·탐험 저장 hash 불변으로 통과했다.

귀가→식사1521.068ms, 공동 식사8244.399ms. 식당12명·침실2명·동료 기상 완료 후day2 Explore 및 전투 승리 후 던전 재개 회귀 pass. 설치물1,701,272bytes, EXE SHA-256 `7ece4c0b24e1459ad144a4bb9e17365900e9a1825cbbed3d8ba2e440630c7951`. 배포 ZIP은 이 Windows CI 설치물의 정확한 EXE/선택 자산을 사용하며 ROM은 제외한다. 문서 결과 갱신은 런타임8decf4f 이후 별도 커밋으로 남긴다. 상세 증거: docs/validation/2026-10-13-default-launch-summary.json.
