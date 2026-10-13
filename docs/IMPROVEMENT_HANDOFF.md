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

## 단계2 결과
- 발견/실방문 mask·방문 방·계단 기억·목표 실패/정체 저장. 같은 방과 지형 경로2행동 주변을 발견(원작 room/corridor 모델의 이진 맵 적용).
- 알려진 도달 가능 계단 > 발견 영역 경계 > 보조 목표. 경계로 접근하여 새 시야를 확보하므로 미방문 전체 순회 제거. 점유로 막혀도 계단 기억 유지.
- 경로 다음 타일 재검사·즉시 재계획, 목표 정리 자체는 이동 행동/시간 미소비. 실제 계단 도착은 이전 목표와 무관하게 층/클리어 처리. NPC는 인접 목표로 접근.
- MDES3 writer·MDES1/2 reader. 기본 누락 필드는 방문 기록에서 복원하며 과거 점유 타일 목표는 재계획. 호환 fixture 검증.
- CTest5개 pass, 10,000 geometry seed·1,000 탐험·원작 던전 회귀 pass. Windows x64 compile/link pass.
- 이전 '모든 아이템 수집', '열매가 항상 목표 최우선' 검사는 사용자 계단 우선 명세에 맞게 변경했다. blocked frontier 앞의 발견 경계 접근으로 조기 귀가 문제를 수정했다.

## 단계3 결과
- actionRound/playerActionRound 및 각 배우 actionRound로 플레이어→적 instance 순서를 처리. 이동/대기/공격/열매 사용은 소비, 방향은 비소비. 적 이동400ms 완료는 추가 공격권을 만들지 않는다.
- player(3,3)→(4,3), enemy(5,3) 고정: EnemyTurn. player(3,3) 고정, enemy(5,3)→(4,3): PlayerTurn. 양쪽 player(3,3)→(4,3), enemy(6,3)→(5,3) 소비: 다음 round PlayerTurn. 원작은 플레이어 이동 직후 인접이면 적 공격 가능하며 거리 선공 예외는 추가하지 않았다.
- Idle/Chase·마지막 발견·목표·막힘·방향·배우별 잔여 적중수 저장. 방/통로 발견 시8방향 BFS로 추적하고 도착 점유 검사. 막힘3회 이후 추적 종료/대기. 이동은 공통 Motion·보행 Animator 연결.
- 공통 canAttack의 8방향·지형 코너 사용. 기존 명중/치명타/피해/열매 유지.
- CTest5개 pass(18.07s): 세 소비 상황·대각선 추적 좌표·이동 중 완료 신호 무시·MDES2 안전 기본값. Windows x64 compile/link pass. GUI에서 새 추적 실측은 미실행.
