# 기상 수면 자세·퇴실 후 페이드

아침 BedFadeIn의 Idle7을 Sleep13으로 바꾸고 주인공/동료가 모두 원본 finite clip의 마지막 자세를 유지하도록 공통 holdGameplayPose를 사용한다. 수면 시작을 다시 재생하지 않으며 이후 Wake14와 두 배우 인사를 순서대로 수행한다.

기존 방 안 퇴실 경로가 끝나면750ms MorningFadeOut을 거쳐 검정 화면에서만 다음날/HP/새 탐험을 한 번 commit하고750ms ExploreFadeIn을 거친다. 퇴실 fade에서는 주인공·카메라가 통로 끝(104,184)에 고정되고 인사를 재생하지 않는다. fade 중 탐험 이동/재계획을 차단하고 양쪽 저장 복원을 검사한다. 기존 enum10/11과 MDES6를 재사용한다. MDES3/4의 이전 퇴실 전 MorningFadeOut은 현재 퇴실 경로로 이어 읽는다.

원본 두 배우 Sleep13 마지막 frame·Wake14 순서, 퇴실 후 끝 좌표/Idle·day/HP commit 시점·fade-in 동안 상태 불변과 저장 복원, 기존 생활/탐험/전투/반복 우회를 포함한 Release11/11(26.75초)을 통과했다. 중복 지형 journal 추가로 과거 defeat 검사의 “빈 journal” 가정을 “기존 journal 불변”으로 갱신했다. 다음은 sanitizer 완료 확인과 실제 Windows 화면/새 trace 검증·EXE 통합 갱신이다.
