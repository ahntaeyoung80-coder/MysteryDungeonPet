# 수동 Reset·저장 복원·Windows 검증 완료

구현605c300에서 우클릭 `Reset 탐험 (귀가)`를 연결했다. 기존 snapshot을 `.reset-backup`에 보존하고 Reset 결과를 durable save한 뒤 화면에 공개한다. 탐험/전투/대화만 허용하며 미확정 공격과 이동을 취소하고 확정 HP·아이템·기록을 유지한다. 귀가/식사/다음날은 기존 생활 루프와 onceKey를 재사용한다. 자동 반복 복구에도 백업과 표시 초기화를 적용했다.

CI38674804212 core/windows 모두 success다. Windows Server2025/MSVC19.51 native11/11(16.53초), 원래 적2↔4/플레이어6↔8 배치의 유한 진행, 저장된 반복 상태의 자동 귀가1회, 실제 우클릭 메뉴/Reset 명령2회/백업/재실행·귀가1회/식사1회 검사를 통과했다. 기존 이동31시나리오(2,341합성frame·2,082제출), 생활·전투·설치/루트 EXE 다른 CWD 기본 실행·복원도 pass다.

Linux CI ASan/UBSan10/10(180.76초)·Python26/26(16.033초, skip없음) 통과. 로컬 전체 sanitizer10/10과 최신 Reset 회귀1/1, Python26/26을 보존했다. 실제 사용자 Windows11 화면·OS 사건·장기 성능은 미관측/별도 범위다.

[검증 요약·hash](../validation/2026-10-15-navigation-summary.json)와 navigation-windows의 원본 CSV·메뉴·상태·백업·events를 따른다. 다음 단계는 실행물 hash 최종 확인과 현재 안내/재개 문서 갱신이다.
