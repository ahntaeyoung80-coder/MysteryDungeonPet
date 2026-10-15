# 이동·방향 전환 2단계

pet_movement_tests를 추가했다. 실제 Explorer/Animator/Motion·MDES5 encode/decode로 8방향64행동, 400ms 이전 중복 행동 차단, 연속 보행 시각, 방향 전환, pause/resume·이동 중 복원을 검사했다. 방향 전환 경로17행동에서도 대기 없는 연결을 확인했다. 코어 결함은 재현되지 않았다.

기존 무작위4초/1초 계측의 이동 관찰 공백을 해결하기 위해 --movement-trace를 추가했다. 명시한 경우에만 실제 합성 화면의 위치·camera·clip·frame·alpha·hash·UpdateLayeredWindow 성공 여부를 프레임별CSV에 기록하고 첫/중간/마지막 DIB를 BMP로 남긴다. 기본 실행/저장 정책은 변경하지 않았다. 고정 MDES5 생성은 pet_movement_tests --fixtures package output으로 재현한다.

관련 이동·기존 개선·생존 검사 통과. MinGW14 x64 Release compile/link 및 EXE/source/assets hash 일치. 실제 Windows 실행은 4단계에서 판정한다. 다음은 1/2/3배 카메라 끝단·alpha·합성 좌표 검사다.
