# 2026-10-21 — 다양성 P1 실제 NPC 연결

## 요청·범위·시간

사용자가 P0 다음 작업을 이어서 진행하라고 지시했다. 시작 시 AGENTS/PROJECT_STATE/현재 파이프라인을 읽고 `origin/work`를 명시 fetch해 `fc9951d`와 clean 로컬 HEAD 일치를 확인했다. P1만 진행했고 다음 P2의 상황 대사·P3 사건/방문을 함께 구현하지 않았다.

착수 전 자산 생성/원본 대조15~25분, 런타임 연결15~25분, 관련 검사/EXE10~15분으로 보고했다. a2.yml의 push/PR 전체 CI와 별도 party workflow가 work에는 실행되지 않는 것을 확인했다. 관련 native/Python부터 검사하고 소스·자산·EXE를 함께 push하며 CI는 백그라운드로 확인하는 경로를 유지했다.

실측은 10:10:37 KST부터 검증/근거 정리 10:36:55까지 **26분18초**다. 첫 문서 조회·원격 비교·범위 보고는 앞선 미계측 구간이다. 자산 생성/코드/검사·Wine/문서가 병행되어 순수 단계별 시간을 임의로 나누지 않는다. commit/push와 원격 마감은 아래 별도 기록한다.

## 변경

- 기본 던전15의8 MD와 구형 던전0의5 MD를 모두 등록했다. 비버니 actor124/MD434·가재군 actor115/MD369의 방문용 Walk/몸체·얼굴, 기존 명시 켈리몬383/384를 포함해17 MD다. 기본/구형 MDPE의 출현·아이템·층 바이트는 그대로다.
- 원작 WAN에서 새14개 몸체를 기존 추출/packer/검증 경로로 만들었다. MD425의 스타팅 몸체, 켈리몬383/384는 기존 파일을 재사용한다. 원작 픽셀·marker/anchor/그림자/tick2,730프레임을 대조했다. 필수 Walk/Attack/Hurt/Idle은 원작8방향이며 없는 Nod/Faint를 복제하지 않는다.
- `npcs/selection.json`→MDNC1 catalogue를 추가했다. MD/species/sprite/명시 KAO/talk group/방문 actor/몸체 key·hash/캐릭터 identity를 구분한다. 예: MD237은 species210/sprite257/KAO236이다. 원작 talk group을 한국어 성격으로 바꾸지 않는다.
- Win32는 실제 instance의 originalId를 조회해 몸체를 고른다. 관계별 녹색/보라 켈리몬 대체를 없앴다. 현재 층에 필요한 몸체만 경로별로 공유하고, 다음 상태의 자산을 먼저 검증·준비해 저장 성공 후 교체한다. 제거 전 해당 Package를 참조하는 enemy/NPC Animator를 해제한다. 매 frame PNG를 다시 읽지 않는다.
- 실제 NPC 발화만 동적 `DialogueActor`를 compositor에 넘기며 플레이어/동료/알람은 기존 선택 cast를 유지한다. 얼굴 하나·명찰 없음·기존 UI/fade·원래 완료 문구를 유지한다. 결손은 같은 캐릭터 Normal이며17개 모두 Normal을 확인했다. 원작 감정 mask와 실행 보유 mask를 구분해 켈리몬의 기존 검증 보완11감정을 보존한다.
- 실제 불꽃숭이의 대화→이동에서 드러난 `direction phase incompatible`를 수정했다. Idle/Nod/다른 주기를 Walk 방향 전환처럼 사용하지 않는다. 같은 clip/loop/주기일 때만 `tryChangeDirection`으로 위상을 유지하고 호환되지 않으면 정확한 동작을 시작한다. 원작 frame/tick·행을 바꾸지 않았다. 기존 엄격한 `changeDirection` 계약과 alias 자체 행을 유지한다.
- 기존 gameplay 전체 복사/설치 경로를 재사용했다. 루트 EXE/source/assets 해시를 함께 갱신하고 설치본도 별도로 실행했다. [P1 계약](../EVENT_VARIETY_NPCS.md), [상태](../../PROJECT_STATE.md).

## 검사·실패·재실행 근거

1. 새 catalogue/기존 캐릭터/대화 native **3/3**, 관련 ASan/UBSan **3/3**이 통과했다. 전체 MD 참조,0/15 양수 출현 전수,관계/instance 비간섭,필수8방향/잘못된 몸체·ID·schema/잘림 거부,Normal fallback,실제 NPC 한 화자 overlay를 검사했다. 기존21종/420개 조합·성별·역할 반전·나옹/먹고자도 재사용했다.
2. Python 최종 **11/11**이 통과했다. actual ROM MD/species/sprite/tbl_talk,명시 KAO와14감정의 원작 또는 같은 Normal 픽셀을 대조했다. 초기 새 검사1개는 helper가 반환한 verification에 추가 MD 메타데이터가 있다는 사실을 무시하고 두 dict 전체를 비교한 fixture 오류였다. 실제 검증 필드를 각각 대조하도록 고친 뒤 통과했다. 최초 준비 코드의 KAO `n_entries()`/MESSAGE tbl_talk API 차이도 실제 모델·inventory를 읽어 수정했다. 최초 요약의 프레임 합계 필드명 오류는 보고 코드만 고쳤으며 원작 프레임 검증 자체는 성공 상태였다.
3. 같은 입력으로 자산을 재생성해 실행 catalogue·몸체/얼굴 바이트와 감사 JSON 의미 일치를 확인했다. ROM hash는 전후 동일하다. 스타팅 몸체·기본/구형 MDPE는 바꾸지 않았다.
4. 첫 EXE batch의 불꽃숭이 친화 대화 종료에서 실제 재생 오류가 발생했다. 실패 trace/이벤트를 보존했고 성공으로 처리하지 않았다. 관련 NPC/core/movement **3/3**으로 수정 회귀를 먼저 고정한 뒤 불꽃숭이 EXE6초 재검사를 통과했다. 공통 Animator 변경이 추가되어 전체 통합/메모리 검사를 한 번 선택·보고했고 **ASan/UBSan19/19,85.20초**가 통과했다. 같은 코드의 전체 검사를 반복하지 않았다.
5. 수정 EXE를 Wine10/Xvfb에서17 MD×우호/적대 **34개**로 전부 다시 실행했다. native Windows 형식 NPC/character/dialogue target3개도 통과했다. 실제 제출 화면의 NPC 불투명 픽셀은 각135~403개가 해당 원작 package frame/anchor/방향과 일치했고,17개 화자 얼굴40×40 전체가 실제 MD/감정 픽셀과 같았다. 요청/실제 감정 trace와 동일 MD2개의 캐시1개 공유도 대조했다. 불꽃숭이뿐 아니라 전 배우에 영향을 주는 재생 공통 수정이므로 전체34개 실제 장면을 다시 확인했다.
6. 실제 원작 던전15의 층 전환6개를 강제 저장 fixture로 실행해 다음 층과 캐시 교체를 확인했다. 캐시는 최대3개였고 최종 이벤트에는 오류가 없다. 기본 실행은 길드 아침→탐험, 일반 재시작은 길드 아침을 유지했다. 각 fixture는 개발 검사 전용이며 제품에 사건을 강제로 발생시키지 않는다.
7. 선택 설치본은 **10,872,989bytes**다. 설치 EXE와 전체 선택 자산 바이트를 원본 출력과 대조하고 실제 설치 EXE에서181회 화면 제출·저장을 확인했다. 기본 Wine 실행 working set 최대는 **42,835,968bytes**이며 앞선 P0 Wine 표본은40,525,824bytes였다. 시나리오/배우가 달라 엄격한 A/B 비교가 아니며 Wine private bytes0은 실제 Windows 측정의 대체가 아니다. RAM 목표/장기 성능 통과로 보고하지 않는다.
8. GUI는 test-only WTS unlock와 NanumGothic 대체를 사용했다. Wine 영속 server가 이미 실행 중일 때 `wineserver -p`가2를 반환해 batch가 검사 전 중단된 준비 문제는 중복 초기화 호출을 제거했다. 최종34개와 층/일반/설치 실행은 정상 종료했다. 캡처를 root 소유 build 하위 폴더에 쓰려던 변환은 agent 소유 previews로 옮겼다. 제품 소스 문제와 구분한다.

[검증 요약](../validation/2026-10-21-event-variety-p1/summary.json), 관련/전체 ASan/Python/Wine 로그·실패 근거·검증기·GUI driver·대표 화면3개를 같은 폴더에 보관했다. 증거 텍스트의 CRLF만 LF로 정규화했다. 루트 EXE SHA256은 `06783e168cb1d309147cc32cf46dd8ae40da27e502c028e58f34eede7fe09ced`다.

## 재개 후 기존 Windows fixture 호환

사용자의 작업 재개 지시 후 AGENTS/PROJECT_STATE와 기존 검증을 확인하고 `origin/work`를 다시 fetch했다. 로컬/원격은 계속 `fc9951d`였다. 관리형 환경 runtime/network/Docker 지침과 현재 정책도 점검했다. 수정5~10분·관련 검사/EXE5~10분·기록/원격약5분을 사전 보고했다.

기존 GUI export가 synthetic MD4를 저장해 새 실제 자산 lookup에서 거부될 수 있음을 발견했다. 순수 core의 합성 fixture는 유지하고 companion/relationship/party/navigation/entry의 **GUI export만** 명시 원작383/384로 바꿨다. 몸체 픽셀 검사도 관계별 그림 대신 trace의 instance→MD와 catalogue 경로를 조회한다. 제품 코드/자산/EXE는 앞선 검증본과 같아서 전체 ASan/UBSan을 반복하지 않았다.

- 관련 native **4/4,21.28초** 통과. native가 내보낸16개 snapshot/17개 NPC와 Windows exporter의24개 snapshot/24개 NPC 전부 실제 catalogue/몸체/얼굴 검증을 통과했다.
- 루트 EXE의 실제 Wine 실행 **9개**가 통과했다. 동료 추종/공격, 우호/적대의 일반 저장·복원, 길 찾기 detour, NPC 접근·인사, 양쪽 적의 순차 공격·한쪽 제거 후 나머지 진행·최종 양쪽 제거를 trace로 확인했다. 동료264개/NPC286개 불투명 픽셀과 관계 복원 전후 몸체가 원작과 모두 일치했다.
- 첫 live trace 조회는 실행 중인 CSV의 미완성 마지막 행을 읽어 임시 보고 Python에서 TypeError가 났다. 실행 완료 후 전체 행을 읽는 검증기는 통과했다. 제품/최종 검사의 실패와 구분한다.
- [재개 근거/검증기](../validation/2026-10-21-event-variety-p1/legacy-fixtures/summary.json), [관련 검사](../validation/2026-10-21-event-variety-p1/resume-tests.txt). 최초 문서의 층 전환 캐시 최대4개 표기는 실제 측정 JSON의 **3개**로 바로잡았다.

실측 재개 구간은12:25:06 KST부터 별도 마감 기록까지다. 앞선10:36:55 이후 사용자 재개 지시 전의 중단과 사전 조회 미계측 구간을 순수 작업 시간에 합산하지 않는다. 검사/문서가 병행되어 단계별 시간을 임의로 나누지 않는다.

## 남은 범위·재개

P1의 기본/구형 출현 몸체·얼굴과 방문용 자산 준비를 마쳤다. 실제 비버니/가재군 자동 방문은 P3이며 새 대사/조건별 문답은 P2다. 다음은 기존9개 발화 경로를 대사 catalogue/세션에 이관하는 P2다. 원작 전체521 MD 지원·새 맵·교환/보상·휴일·실제 Windows11/OS lock/power·장기 성능은 이번 완료 범위가 아니다. 자동 CI는 push 후 확인하며 미완료를 성공에 포함하지 않는다.

## 커밋·원격 마감

재개 측정12:25:06 KST→코드/fixture/EXE/문서 검증12:30:00 KST까지4분54초이며, 원격 마감은 후속 기록한다. 사전 추정 범위 안에서 P1만 마감한다. 코드/자산/EXE는 일반 push로 CI를 트리거하며 문서 원격 결과만 별도 `[skip ci]`로 남겨 중복 전체 CI를 피한다.
