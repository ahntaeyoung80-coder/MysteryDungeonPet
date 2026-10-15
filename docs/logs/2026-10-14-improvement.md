# P0/P1/P2 개선 제작 기록

사용자 확정 명세의 단계0~7을 work에서 기존 생활·전투와 자산을 재사용해 구현했다. 중단 이후 마지막 미커밋 영역인 단계7부터 재개해 저장 호환·표시 복원·통합 회귀와 실행물 정리를 마쳤다. 원본 ROM·압축본 SHA256은 불변이며 Push는 하지 않았다.

단계별 커밋은 [인수인계](../IMPROVEMENT_HANDOFF.md)의 표에 기록했다. 마지막 구현 커밋은 97cfb8b다. MDES1·2 접촉/완료 NPC fixture, 이동·보행 위상 복원, 배우별 완료, 계단 우선 접촉, 시야를 잃은 추적 종료, 카메라 홀수 viewport를 최종 확인했다.

Release CTest5/5(24.58초), ASan/UBSan5/5(272.24초), Python25/25(20.091초, skip 없음), Windows x64 MinGW compile/link와 모든 PowerShell 검사 스크립트 문법 통과. 원본 소품 bytes·전체 배경·밤 창틀 보존도 원본 비교로 확인했다. [검증 요약](../validation/2026-10-14-improvement-summary.json)과 함께 검사 로그를 저장했다.

실행 ZIP: local/MysteryDungeonPet-improved-windows.zip, 1,598,796bytes, 77개 파일, 압축 해제5,038,566bytes. 원본 ROM·압축본·저장을 제외했다. ZIP CRC 검사 통과이며 실행 파일 SHA256은 검증 요약에 있다.

남은 확인은 새 코드의 실제 Windows GUI·육안 인수, OS 잠금/절전/정지·재개, 장기 메모리/CPU다. 이전 CI 성공을 새 실행 증거로 쓰지 않는다. 다음 시작은 tools/run_windows_checks.ps1이며 재현 위치와 대상 함수는 인수인계에 기록했다.
