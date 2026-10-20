# 2026-10-20 — 명찰 제거·맵/층 표시·원작 정상 완료·식사6초

## 범위·예상·시간

사용자는 명찰 제거, 각 맵의 원작 종료 장소에서 성공 문구 후 귀가, 우상단 `[맵이름] - 1F` 표시를 요청했다. 진행 중 식사 장면의 마지막1초를 잘라 기존7초에서6초로 하되 애니메이션을 압축하지 않도록 확정했다. 이전 화자 단독 표시·캐릭터 ID 교체 점검은 유지한다.

08:01:38 UTC 착수 때 원작 종료 장소 조사5~10분, 구현15~25분, 관련 검사·실제 EXE5~10분을 보고했다. 08:25:10까지23분32초 경과했다. 자료 조사·구현·관련 검사·Wine 실행과 문서 작성이 겹쳤으므로 항목별 순수 시간을 임의 분리하지 않는다. 전체 CI 완료는 배포 조건으로 두지 않고 push 뒤 백그라운드다. 로컬의 origin/work 추적 ref가6e54e2d에 남아 있어 조사 후 확인하니 fetch refspec이 main만 추적하고 있었다. 명시 fetch의 FETCH_HEAD와 연결된 GitHub ref로 실제 원격을 확인했으며, work 추적 refspec을 추가했다. 원격 push는 기존 b48fb0c→이번18d236d로 반영됐다.

## 원작 확인·구현

- 현재 기본 dungeon15 Upper Steam Cave는 MAPPA12 start8의7층 구간이다. 다음 원작 dungeon16 Steam Cave Peak는 같은 그룹 floor15, fixed floor3이며 `SCRIPT/D10P41A/m10a0901.ssb`의 banner16/ground와 연결된다. BMA/BPC/BPL 배경, SSA 시작 좌표(주인공344,364/동료304,348), 첫 SSB 이동 목표(344,228/304,228)를 추출했다. 원본 ROM hash는 변경되지 않았다. 고정 SkyTemple의 palette iterator를 Pillow12 crop 전에 byte 배열로 materialize하는 오프라인 호환 처리를 적용했다. 별도 폴더 재추출5파일이 모두 byte-identical이다.
- 마지막 계단에서 clear를 한 번 기록한 뒤 정상 fade-in→2초 이동→두 배우의 원작 Pose17→정확한 문구 `탐험을 성공적으로 마쳤다`→fade-out→길드 fade-in으로 연결한다. 원작 보스/스토리 VM은 실행하지 않는다. 던전0은 원작 Test Dungeon이며 공식 종료 장소가 없어 현재 지도에서 완료 문구를 보여준다. 현재 배포하지 않는 나머지 ROM 맵 전체의 종료 장소는 구현 범위가 아니다.
- 던전 ID별 종료 registry와 ROM hash를 검사한다. 정상 장면은 표시 좌표만 사용하고 논리 계단 위치·획득품·RNG를 변경하지 않는다. 정상의 카메라로 탐험 노출 마스크를 기록하지 않는다. MDES10은9의 필드 배치를 유지하면서 beat20~24와 deadline을 저장하며1~9도 읽는다. 구형 clear pose도 정상 연출로 이어진다.
- 명찰 그림·GDI 이름 출력을 제거했다. 현재 화자 한 명의 원작56×56 프레임/40×40 초상화와 좌우 위치는 유지한다. 역할/instance/ROM 캐릭터 binding은 유지하며 몸체/초상화 교체 회귀를 재확인했다.
- 우상단 맵·층을 한 줄로 표시한다. 긴 이름만 줄이며 `] - nF` 접미사는 남긴다. HP와 겹치지 않게 위에서24px 아래에 오른쪽 정렬한다. 식사는 속도·tick·프레임·카메라를 바꾸지 않고 시작6초에 DiningExit로 전환하며 구형7초 저장도6초로 이행한다.

## 검사·수정·결과

- Linux 관련8개 target: completion_scene, dialogue_portrait, package_transition, companion, entry_social_scene, survival, improvement, gameplay. 최초 완료 좌표 보간에서 음수 signed 값과 unsigned duration의 나눗셈 때문에 좌표가 넘치는 실패를 발견했다. 분모를 signed로 맞춘 뒤 completion/gameplay2/2 통과, 저장 이행 변경 후 completion/package_transition/improvement/gameplay4/4 통과, 최종 식사 이행 assertion 추가 후 completion/gameplay2/2 통과했다. 변경과 무관한 통과 결과는 재사용했다.
- 완료 검사는 단독/동료·던전15/0, 중간 계단·Reset의 성공 배제, 각 beat와 미완료 동료 기다림, 저장/복원 replay, 원래 좌표·HUD ID, clear/획득품/RNG 중복 방지와6초 경계를 확인했다. 기존 생존/패배 회귀도 통과했다.
- MinGW GCC14.2 posix/static x64 Release를 빌드하고 Wine10/Xvfb에서 실제 EXE7개 실행이 모두 exit0이었다. 기본 정상과 테스트 던전 완료, NPC·동료·취침·기상/노공룡, 플레이어/동료 교환을 실행했다. 화자 초상화9개 캡처의 원작 그림/프레임·명찰 제거, 정상 배경 원본 픽셀·이동 좌표·순차 완료/귀가를 대조했다. 식사 첫 제출→퇴장 첫 제출은5999.289ms로 프레임 계측 오차 안에서6초다. 상태 검사는 정확히5999/6000ms 경계를 확인한다.
- GDI 실제 한글8장면(성공 문구 포함)과 맵/층3문구의 alpha/영역을 확인했다. 최종 EXE·루트 EXE·windows-runtime manifest·캡처 증거의 SHA256이 일치한다. 원작 자산5파일 재추출도 일치한다. `git diff --check`, 루트 EXE/source/assets hash 검사가 통과했다.
- 전체 메모리/전체 통합 검사는 실행하지 않았다. 이번 변경은 완료 상태 저장·장면에 관련된 회귀와 실제 EXE를 먼저 확인하는 사용자 배포 지시를 따른다. 최종 EXE 로그에 이미 준비된 HomeScene도 기록하도록 조정해 기존 native GUI 검증 계약을 유지했고 최종 EXE7실행을 다시 확인했다. native MSVC/사용자 Windows11과 push CI는 이 로그 작성 시 pending이다.

검증: [요약](../validation/2026-10-20-completion-hud/summary.json), [원작 연결](../validation/2026-10-20-completion-hud/source-audit.json), [최종 완료/식사 검사](../validation/2026-10-20-completion-hud/completion-gameplay-tests.txt), [저장 이행](../validation/2026-10-20-completion-hud/snapshot-transition-tests.txt), [GDI](../validation/2026-10-20-completion-hud/gdi-tests.txt). 다음은 사용자 Windows11 표시 확인과 B의 남은 인수 기준이다.

## 원격 반영·실측 마감

08:26:18 UTC에 기능18d236d의 push CI가 시작됐다. 08:26:19 조회 기준 착수부터24분41초 경과했으며 실제 GitHub work ref가18d236d7356fc8e9acc713f86edd4396b3739103인 것을 연결 도구로 확인했다. [CI39393393693](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/39393393693)는 in_progress로 완료를 기다리지 않는다. 이 마감 기록만 별도 docs commit `[skip ci]`로 반영해 전체 CI 중복 실행을 막는다. 별도 CI 대기 시간은 없고 진행 중 로컬 검사·빌드·Wine 대기는 위 실측 경과에 포함된다. GDI 캡처의 CRLF 공백 검사를 정규화한 뒤 diff 검사도 통과했다. 로컬 재사용 도구 컨테이너는 삭제하지 않고 정지한다.
