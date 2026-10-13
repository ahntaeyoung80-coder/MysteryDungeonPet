# 개선 제작 인수인계

## 확정 범위
2026-10-13 사용자 명세: P0 8방향 연속 이동·카메라·계단 우선, P1 소비 기반 턴·추적·8방향 공격·HP5·사망, P2 원본 소품·식당 이동·취침/기상·NPC 대화·하트 확대. 단계0~7별 검증·로컬 커밋. Push는 별도 지시 전 하지 않는다.

## 기준 확보 (단계0)
- 기존 브랜치 work / HEAD70eb4cf, 미커밋 변경 없음.
- 원격 work3cbb95b, 최신 생활·전투 fa659271. 기존 HEAD는 최신의 조상이므로 브랜치 전환·reset 없이 work를 fa659271로 fast-forward했다.
- ROM SHA256: 6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf
- rom-files.zip SHA256: cc9d113ea4b438d8b8895a30c3d47863dc99cdb1e1381ce8c029aeeb8d748b54
- 원본 두 파일은 수정 금지. 기존 C++ 코어·GUI·자산 준비 도구 재사용.
- 환경: Linux/GCC14, cmake·Python 고정 의존성 준비. Windows11 직접 실행 불가. 새 코드는 교차 컴파일과 코어 회귀로 검증하고 실제 Windows 결과와 구분한다.

## 다음 작업
1단계: exploration.cpp bfs/공통 이동·공격 판정, animator 방향 위상, 공통 표시 보간, Windows camera/render/deadline. 원작 참고 fc850408의 dungeon_capabilities_4.c, dungeon_range.c, overlay_29_023047B8.c를 local/reference에 읽기 전용 참고로 확보했다.

## 단계1 결과
- 원작 방향0~7·8방향 BFS(대각선 한 행동), 지형 코너와 도착 타일 점유 분리. 이진 일반 바닥에서 이동/공격 모두 직교 양옆 벽 차단.
- Motion 공통 표시 구간(actor/action/from/to/monotonic/duration/direction), 24틱/400ms, pause/resume. Windows 플레이어·카메라·표시 deadline 연결. 적 이동은 단계3에서 같은 모듈로 연결한다.
- 보행 방향 변경은 Animator 시작 시각/marker 상태 유지. 픽셀 위치는 표시 때만 반올림.
- 기준선 CTest4개 pass, 변경 후 CTest5개 pass(29.05s), Windows x64 MinGW compile/link pass. 실제 Windows GUI의 부드러움은 아직 미실행이다.
- 기준 MDES1/2 fixture는 기존 writer로 생성하여 후속 호환 검증에 보존했다.
