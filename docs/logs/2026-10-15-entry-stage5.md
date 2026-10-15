# 실제 Windows·EXE·인수인계 완료

구현0081db8의 CI38697951696 core/windows 모두 성공했다. Linux 최종 ASan/UBSan11/11(97.84초)·Python26/26(10.129초), Windows Server2025/MSVC19.51 native12/12(18.67초) 통과다.

실제3층의 최초 fade-in에서 entry439/stairs210·플레이어439와 표시(180,444)가 일치하고 entry entity0, 즉시 클리어 없음이었다. 경로 밖 NPC 인사/응답은1회다. 아침 fade의41제출frame에서 두 배우 clip13/frame1(각 원본 마지막 frame)을 유지했다. 퇴실 fade44제출frame에서(104,184)를 유지했고 RGB합12,523,509→1,184로 어두워졌다. 탐험 fade46frame은7,486→18,881,839로 밝아지며 이동을 막았고 day2/HP5를 한 번 commit했다.

기존 이동31시나리오(2,414합성frame·2,118제출)·이동 중 종료/복원·반복 우회/수동 Reset/백업/중복·생활/전투·설치/루트 EXE 다른 CWD 기본 실행/복원도 통과했다. [요약·hash](../validation/2026-10-15-entry-summary.json)에 artifact11954839440과 원본 CSV/상태/스냅샷/events를 보존했다.

루트 EXE/manifest는 CI 구현 그대로이며 source/assets hash가 일치한다. 원본 ROM/ZIP 불변, 새 자산 추출/배포 ZIP은 없다. 현재 상태·명세·결정·실행/직접 수정 안내·MDES6 계약·인수인계를 갱신하고 링크/증거/실행물 hash와 diff를 확인한 뒤 최종 문서 커밋을 work에 push한다. 로컬/원격 SHA 및 clean 상태로 재개 지점을 확인한다. 실제 사용자 Windows11·OS 사건·장기 성능·전체 콘텐츠는 별도 범위다.
