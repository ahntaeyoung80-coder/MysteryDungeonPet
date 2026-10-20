# 2026-10-20 — 층수/최종 fade·상호 대화·사망 대사·우클릭 스타팅/감정

## 범위와 시간

사용자의 후속 “작업하라”로 앞선 검증 범위를 구현했다. work ed032f2에서 원격/로컬 일치와 깨끗한 작업 트리를 확인했다. 착수21:30:52 KST, 예상 구현25~40분/관련 검사와 실제 EXE10~15분을 보고했다. 21:59:40 KST 최종 EXE·증거·source/assets hash 대조까지28분48초였다. 구현·변환·검사·Wine 실행·문서가 겹쳤으므로 항목별 순수 시간을 만들지 않는다. CI 대기는 배포 조건으로 두지 않고 push 후 백그라운드다.

## 구현

- 우상단은 `1F`처럼 층수만 하트와 같은 줄에 표시한다. GDI 글자를 포함해 배경/HP/프레임/초상화를 합성한 뒤 불투명 검정 fade를 한 번 적용한다. 제출 캐시에 opacity도 비교해 정적인 화면에서 fade가 멈추지 않는다. blackout 안내만 검정 화면 위에 표시한다. 진단 trace의 hash/rgb_sum도 최종 DIB 기준이다.
- 플레이어↔NPC/동료의 실제 표시 방향을 저장된 상대 위치에서 구한다. NPC 대화의 동료 청자도 NPC를 본다. 애니메이션 phase를 유지한 방향 변경을 사용한다.
- 사망 시 동료의 고정 대사를 원작 대화창/해당 감정 초상화로2400ms 표시한 뒤 기존 fade/blackout/귀가로 잇는다. 주인공 기절은 걱정, 동료 기절은 슬픔/마지막 대사를 구분한다. 대사에 안정된 context/line/화자/감정 ID를 부여했다. 현재 랜덤 선택은 하지 않으며 전투/지도 RNG도 소비하지 않는다. MDES11에 DeathStage2를 추가했으며 필드 배치는 유지하고1~10을 읽는다.
- 원작 overlay13의 역할별 스타팅을 사용한다. 주인공19종/동료21종, 성별34 MD/22몸체이며 나옹/먹고자는 동료 전용이다. 우클릭 하위 메뉴는 허용 목록만 보여준다. 사용자 MDCP1 `characters.state`를 atomic 저장하고 몸체 로드 전에 적용한다. 잘못된 선택/ROM/프로필은 기본 cast를 유지하며 같은 종의 두 역할도 구분한다.
- 메뉴 선택 시 후보의 필수13동작·몸체/초상화 hash를 먼저 검증한다. 대화/공격/기절/클리어 중에는 예약하고, 이동에서는 동일 Motion/행동 시각의 Walk를 유지한다. 이전 Animator/길드 Package 참조를 해제하고 두 몸체·cast를 함께 바꾼다. HP·위치·아이템·날짜·행동권·RNG를 변경하지 않는다.
- 스타팅의 원작 감정14종을 연결하고 잘못된 켈리몬 KAO373(릴리요)을 정확한 KAO382로 교정했다. 켈리몬11/노공룡8의 부족 표정은 고정 SpriteCollab commit의19PNG로 보완했다. 원작 표정은 유지하고 소스/기여자/조건/파일 hash를 보존했다. 선택 배포 폴더에 credits 및 CC BY-NC4/PMDCollab1 문서를 포함한다. 루트 runtime hash 검사도 Markdown 라이선스의 CRLF를 정규화해 Windows checkout과 일치시킨다.
- 오프라인 준비 명령은 `python -m tools.prepare_starter_characters --out <새 경로>`다. 기존 body exporter/packer/원본 검사를 재사용한다. 전체103파일을 다시 생성해 byte-identical임을 확인했다.

## 검사·수정

관련 native9개 target(completion_scene, dialogue_portrait, package_transition, companion, entry_social_scene, survival, improvement, gameplay, character_scene)을 사용했다. 첫8개 실행 중 portrait의 대규모 provenance가 기존16KiB 문자열 한도를 넘어 실패했다. 한도를 완화하지 않고 상세 감사는 JSON 파일에, 실행 패키지에는 파일/hash 요약을 넣었다. 실패 target과 새 character_scene2/2를 재실행해 통과했다. 다른7개 통과 결과는 재사용했다.

추가 character_scene 검사는34개 ID의 필수 clip/14표정,19/21 메뉴 제한·금지 ID, 같은 종의 두 역할, atomic 프로필 재저장·복원과 손상 시 전체 기본값 유지, 실제 렌더 방향의32사례/64발화·복원 상태, MDES10 이행, 양쪽 사망 대사의 최소 시간/복원/RNG 보존, 공통 최종 fade를 확인했다. 관련 target만 다시 실행해 통과했다.

Python7개(기존 ID4+실제 원작 그림/성별/보완 표정3)가 통과했다. metadata끼리 맞는지뿐 아니라 원작 KAO382의 실제 RGBA와 현재 켈리몬 그림이 같고 릴리요373과는 다른지 확인해 앞선 잘못된 수동 매핑을 회귀로 고정했다.34개 캐릭터의 원작14표정과 body sprite/dex·성별 alias, 보완19파일 및 남겨둔 원작 표정도 대조했다. GDI 한글8장면/층수3문구가 통과했다.

## 실제 EXE

MinGW GCC14.2 posix/static x64 Release를 Wine10/Xvfb에서 실행했다. 테스트용 unlock helper와 Nanum 글꼴 대체를 Wine에만 설정했고 제품에는 Windows 기본 메뉴 글꼴/맑은 고딕을 유지한다.

- 모든 파트너21종과 암컷 피카츄의 총22몸체가 실제 WIC 로드/렌더/exit0이었다. 원작 코드·에셋으로 native 로더/그림 검증을 마친 각 패키지의 실제 Windows 실행을 추가 확인했다.
- NPC/동료의 발화 전후, 양쪽 기절 대사, 취침/노공룡/기상, 교체 후 재시작의11장면을 해당 캐릭터/감정의 정확한 atlas40×40과 대조했다. 명찰/듣는 상대 초상화는 추가하지 않았다.
- 외부 검사 도구가 다른 프로세스의 HMENU를 직접 읽지 못해 최초 메뉴 계수 시도는 도구 실패였다. 제품의 실제 Win32 메뉴 생성/개수 검사와 실제 popup 키보드 선택으로 대체했다. 주인공19/동료21 메뉴를 열어 피카츄25·나옹52를 선택했고 선택 적용과 재실행 뒤 유지가 확인됐다. 첫 교체 출력의 주인공 opaque 픽셀도 새 피카츄의 정확한 frame/anchor/위치와 대조했다. 메뉴에서 한글은 Wine 시스템 폰트 Tahoma/MS Shell Dlg만 Nanum으로 대체해 읽을 수 있게 확인했다.
- 선택한 리오르489로 원작 정상·성공 문구·귀가·식사 약6초·취침·기상·알람·퇴실·다음 탐험35초를 실행했다. 생활 장면마다 선택이 유지됐다. 최종 빌드의 메뉴 검사/로그 추가 뒤 메뉴·재시작·전체 생활 장면을 확인했고 본문/에셋 로드가 같은22몸체 검증은 재사용했다.
- fade 첫 화면의 HUD 최고 밝기가 `255-opacity`와2이내로 일치했다. 화면의 하트/바닥과 함께 층수도 어두워지고 맵 이름은 표시하지 않는다.

[실행/그림 검증](../validation/2026-10-20-character-settings/summary.json), [native 실패 원인/기존 통과](../validation/2026-10-20-character-settings/related-tests-initial.txt), [수정 target](../validation/2026-10-20-character-settings/related-tests-fixed.txt), [추가 경계](../validation/2026-10-20-character-settings/character-edge-tests.txt), [실제 종/그림 검사](../validation/2026-10-20-character-settings/pixel-identity-tests.txt).

소스·선택 에셋·루트 EXE/manifest와 증거 SHA가 일치하고 diff 검사도 통과했다. 전체 메모리/전체 통합 검사는 이번에 실행하지 않았다. 관련 검사→실제 EXE→소스/EXE 동시 반영을 우선하는 사용자 지시를 따른다. native MSVC/사용자 Windows11은 아직 미확인이고 자동 CI는 push 후 백그라운드다. 다음은 Windows11 실제 표시와 B의 남은 인수 기준이다.
