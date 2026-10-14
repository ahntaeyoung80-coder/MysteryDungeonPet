# 피격·도주·취침/퇴실·NPC 텍스트 수정

기준 work46fdd6a. 사용자가 보고한 피격 선차감, 도주 한 걸음/휴식, 취침 인사 누락·수면 정지 장면, 경로 NPC 무인사, 텍스트 누락, 맵 밖 긴 퇴실을 수정했다.

구현 계약은 docs/implementation/MDES5_FORMAT.md의 최신 절을 따른다. 원작/연결 자산은 변경하지 않았으며 루트 EXE와 windows-runtime.json을 함께 갱신한다. GitHub Desktop pull 이후 더블클릭 실행을 유지한다.

검증 로그는 docs/validation/2026-10-14-interaction-{release,sanitize,python,windows}.txt와 최종 summary를 따른다. Windows GDI test는 생산 렌더러에 한글 문자열6장면을 전달해 실제 흰 글자 픽셀과 ULW_ALPHA의255 alpha를 확인한다. 이전 GUI 이벤트 검사만으로 텍스트 표시를 판단하던 빈틈을 보완한다. 사용자 Windows11 육안 검증은 Windows CI 결과와 별개다.
