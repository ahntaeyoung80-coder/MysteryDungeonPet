# 즉시 구현 명세 반영

기준04082db에서 기존 런타임·연결 자산만 수정했다. 원반 제한, 아이템/frontier/계단 목표 정책과3회 실패 제한, 층 fade/단일 commit, 밤 취침, 주인공 기상·방 이탈, 식사7초를 적용했다. 목표와 점유 변화의 경로 캐시를 분리해 전투 후에도 진행 목표를 보존한다. MDES4는 필드 배치를 유지한 enum 확장이며 이전 저장을 읽는다. 수집 inventory와 pickup 기록은 다음날에도 유지한다.

Release6/6·ASan/UBSan6/6·Python25/25·Windows x64 교차 빌드를 통과했다. 실제7000ms 안에 기존 공동 식사 animation의 한 cycle이 들어가는지도 확인했다. 단계별 저장/복원과 층 전환 전후의 탐험 중단·중복 journal 차단을 회귀로 검사했다. 상세 수치/로그는 docs/validation/2026-10-14-runtime-revision-summary.json이다.

ZIP은 만들지 않았다. 루트 MysteryDungeonPet.exe와 소스·사용 자산 hash 기록을 추가하고, EXE 기준 저장소 경로를 지원했다. Windows CI는 기존 설치 EXE뿐 아니라 루트 EXE도 다른 CWD에서 무인자 실행·저장 복원한다. 사용자 Windows11의 육안·OS 사건·장기 성능과 네이티브 CI 결과는 교차 빌드와 구분한다. 원본 두 입력 hash는 불변이다.

계단 경로에서는 새로 발견된 목표뿐 아니라 이미 발견된 공간의 새 아이템·새로 도달 가능한 우선 목표도 재평가한다. 우선 목표가 없으면 기존 계단 경로/cache를 유지한다. 추가 회귀를 통과하고 루트 EXE/hash를 재생성했다.

첫 Windows CI38516554129에서 MSVC CTest6/6와 WIC·재생 검사는 통과했다. 이전 탐험 검사만 기상 직후/방 이탈 전 HP5를 가정해 실패했다. headless 드라이버의 종료점은 Ready/RoomExit이며 실제 다음날 회복 전이므로, 검사에서 이 단계와 복원 전후 HP·day·RNG·inventory·transaction의 동일성을 확인하도록 수정했다. Windows GUI는 실제 방 이탈 뒤 다음날을 시작한다. CI 결과를 로그에 출력하고 배포 바이너리 ZIP 대신 검사 결과만 artifact로 남긴다.
