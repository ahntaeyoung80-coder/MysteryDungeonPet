# 동료 5단계 — Windows 통합 검증·실행물

소스6ccb38c의 [CI38717413606](https://github.com/ahntaeyoung80-coder/MysteryDungeonPet/actions/runs/38717413606)는 core/windows 모두 성공했다. 첫 통합2fc0809의 CI38716812063도 성공했으며, 최종 실행물에는 켈리몬 유한 응답 동작이 대사 최소 시간을 넘을 때 지난 deadline을 반복 조회하지 않도록 보강했다.

- Windows Server2025/MSVC19.51 x64 Release native13/13(26.97초), Linux GNU13.3 ASan/UBSan12/12(264.21초)·Python26/26(16.973초).
- 실제 Windows 앱6장면: 파이리 1칸 추적/Walk0·독립 Attack1·더 가까운 동료만 피해/Hurt6·방1칸 지원·통로 대기·Faint33/패배/귀가·식사 연결. 모든 실제 저장은 native reader로 다시 검사했다.
- 제출 BMP의 원작 몸체 픽셀은 파이리264/264, 초록색 켈리몬286/286 일치했다. 실제 CSV·상태·픽셀 결과/화면은 `docs/validation/2026-10-15-companion-windows/`다.
- 기존 이동31시나리오·원래 반복 우회/재실행/자동 귀가 없음·수동 Reset/백업·3층 진입/발밑 제외·NPC 대화·수면 마지막 자세·퇴실 fade/다음날·생활/전투·설치/루트 EXE 기본 실행/저장 복원도 통과했다. 신규 party 검사는 기본 모드, 기존 단독 고정 snapshot만 `--solo`다.
- 로컬 Release12/12(23.41초), ASan/UBSan12/12(196.55초), Python26/26(17.108초), MinGW Release/루트 EXE/source/assets hash 일치. 원본 ROM/ZIP·기존 두 배우 자산은 유지했다.
- 실제 사용자 Windows11 화면·실제 잠금/절전 하드웨어·장기 성능은 이번 CI 결과와 구분한다. 전체 hash/환경/원격 증거는 `docs/validation/2026-10-15-companion-summary.json`을 따른다.
