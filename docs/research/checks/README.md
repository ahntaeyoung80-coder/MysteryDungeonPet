# 조사 당시 검사 자료

이 폴더의 Python/JSON 9개는 이전 조사 작업 폴더 /workspace/scratch/8d2e980747a2/checks에서 원문 그대로 복사한 자료다.

- geometry_check.py: 24×24 논리 격자의 2~3개 방, 연결성, 출발·계단 도달성에 대한 10,000개 시드 검사. ROM 지도 재현 검사가 아니다.
- cdf_validation.json: 분포 변환 함수 본문 분리 검사 결과.
- event_validation.json: 스크립트 바이트 경계 및 선택 명령 검사 결과. 게임 재생 검증이 아니다.
- png_validation.json: 공개 PNG/XML 구조 검사 결과. ROM 동일성 검증이 아니다.
- package_validation.json: 보고서 구성·원본 SHA256·참조 검사 결과.
- link_requests.json, link_results.json: 고정 커밋의 소스 경로 확인 요청과 결과.
- trees_a.json, trees_b.json: 조사 대상 저장소 경로 목록.

geometry_check.py는 Python 표준 라이브러리만 필요하며 `python docs/research/checks/geometry_check.py`로 재실행할 수 있다. 나머지 JSON은 이전 검사 기록이며 모든 검사 실행기가 보존된 것은 아니다.
원본 작업 폴더의 외부 소스 복사본, 게임 스크립트/이미지가 포함된 원시 샘플, 문서 작성용 도구 및 다운로드 도구는 포함하지 않았다.
