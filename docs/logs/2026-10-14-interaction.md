# 피격·도주·취침/퇴실·NPC 텍스트 수정

기준 work46fdd6a. 사용자가 보고한 피격 선차감, 도주 한 걸음/휴식, 취침 인사 누락·수면 정지 장면, 경로 NPC 무인사, 텍스트 누락, 맵 밖 긴 퇴실을 수정했다.

구현 계약은 docs/implementation/MDES5_FORMAT.md의 최신 절을 따른다. 원작/연결 자산은 변경하지 않았으며 루트 EXE와 windows-runtime.json을 함께 갱신한다. GitHub Desktop pull 이후 더블클릭 실행을 유지한다.

검증 로그는 docs/validation/2026-10-14-interaction-{release,sanitize,python,windows}.txt와 최종 summary를 따른다. Windows GDI test는 생산 렌더러에 한글 문자열6장면을 전달해 실제 흰 글자 픽셀과 ULW_ALPHA의255 alpha를 확인한다. 이전 GUI 이벤트 검사만으로 텍스트 표시를 판단하던 빈틈을 보완한다. 사용자 Windows11 육안 검증은 Windows CI 결과와 별개다.

구현2f2530d를 work에 반영하고 CI38530667338을 확인했다. core job115741517846/windows job115741517320 모두success. Windows MSVC CTest8/8(16.34초), 실제 GDI text ink/alpha 검사, 밤 인사→잠들기→fade→아침 Wake/인사→1870ms 퇴실/다음날 GUI, 전투, 설치본/루트 EXE의 다른 CWD 무인자 실행과 동일 runId 복원이 pass다. CI Linux ASan7/7(171.74초)/Python26/26(16.561초)도 pass다. 검사 로그의 짧은 던전 이동 관찰은 대화/전투로 보류였으므로 도주 화면을 실제 사용자 Windows11에서 직접 본 것으로 확대하지 않는다.
