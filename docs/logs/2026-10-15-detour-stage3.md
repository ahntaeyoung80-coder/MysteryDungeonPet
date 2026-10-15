# Windows 우회·실행물·인수인계 완료

구현1babcf2의 CI38692398718 core/windows가 모두 성공했다. 최종 소스의 Linux ASan/UBSan10/10(123.13초)·Python26/26(18.389초, skip없음), Windows Server2025/MSVC19.51 native11/11(16.64초) 통과다.

실제 Windows 앱의 저장된 반복 상태에서 첫 이동은77→54였다(기존 정상 경로의 다음 칸102와 다름). 동일 run900/floor0에서 탐험을 계속했고 재실행 후에도 귀가/자동 Reset0회·HP2·획득 상태를 유지했다. 수동 Reset의 실제 메뉴·백업·명령2회/귀가1회·재실행 후 식사1회는 통과했다. 기존 이동31시나리오의2,386합성frame/2,093제출·이동 중 종료/복원과 생활/전투·설치/루트 EXE 다른 CWD 기본 실행/복원도 통과했다.

[검증 요약·hash](../validation/2026-10-15-detour-summary.json)에 CI artifact11953130820의 SHA256과 원본 CSV·events·snapshot·결과를 보존했다. 루트 EXE/windows-runtime.json은 이 CI 구현 그대로이며 source/assets hash가 일치한다. 원본 ROM/ZIP은 불변이다.

README·현재 상태·사용법·설계 결정·MDES6 계약·인수인계/임시 문제를 같은 층 우회와 수동 Reset 기준으로 정정했다. 이전 자동 귀가 기록은 폐기된 정책의 이력으로 구분했다. 문서 링크·증거/실행물 hash·git diff --check를 확인한 뒤 최종 결과를 work에 커밋/push하고 로컬/원격 SHA 및 clean 상태를 확인한다. 실제 사용자 Windows11 화면·OS 사건·장기 성능·전체 콘텐츠는 별도 범위다.
