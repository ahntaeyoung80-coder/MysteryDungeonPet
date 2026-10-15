# 카메라·외곽 표시 3단계

독립 viewport crop 기준과 실제 compose 출력을 픽셀 단위로 대조했다. 1/2/3배, 네 변/모서리·clamp 시작/끝·반픽셀을 포함한363화면의 배경·배우·소품 위치와 40,000픽셀 alpha255가 일치했다. 정지 후 같은 화면을 다시 합성해 byte 불변과 제출 불필요를 확인했고, 카메라 중앙 추적의 반픽셀 샘플에서도 anchor가 흔들리지 않았다. 합성 fixture를 실제 원작/Windows 육안 증거로 확대하지 않는다. 카메라 결함은 재현되지 않았다.

pet_camera_tests/pet_movement_tests 통과. Windows 원작 합성 trace를 같은 좌표·alpha·중간 위치·방향·연속 행동 계약으로 판정하는 tools/check_movement_trace.py를 추가했다. Python 문법 검사와 MinGW 빌드·runtime hash 일치 확인. 실제 Windows trace 판정은4단계에서 실행한다.
