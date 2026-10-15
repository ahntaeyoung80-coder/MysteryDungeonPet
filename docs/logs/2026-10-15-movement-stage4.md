# 이동·카메라 통합 회귀 4단계

## 로컬 실행

- Linux GCC14.2 Release9/9(31.79초), ASan/UBSan9/9(132.31초, leak검사 포함) 통과.
- 최신 snapshot inspect 코드까지 재빌드한 이동·카메라 sanitizer2/2(8.51초) 통과.
- 고정 의존성 Python26/26(21.072초, skip없음), PowerShell 전체 문법 검사 통과.
- MinGW14-posix x64 Release compile/link, 루트 EXE/source/assets hash 일치. ROM/rom-files.zip SHA256 불변.

## 실제 Windows 검증 경로

기존 앱에 정상 MDES5 입력을 제공한다. 새로운 이동/카메라 알고리즘을 시험 전용으로 구현하지 않는다. 네이티브 MSVC 앱·원작 패키지·WIC·Motion/Animator·합성·UpdateLayeredWindow를 통과한 프레임을 기록한다. 1/2/3배의 직선·전환6개와 네 변/모서리24개, 실제 프로세스 kill 후 snapshot 재실행을 포함한다. 계측은 --movement-trace를 명시한 경우에만 켜진다.

최초 CI38665802412는 네이티브10/10과 직선 이동을 통과했지만 전환 시나리오의 관찰5초가 첫 방향 전환5.2초보다 짧아 실패했다. FIFO BFS의 동일 최단거리 경로를 그대로 사용하고 관찰을6초로 늘렸다. 계단 도착6.8초 전까지 관찰해 fade와 분리했다. 두 번째 CI38666143525에서는31개 프레임 검사(2,400frame/2,105제출)가 통과했지만 복원 판정식의 PowerShell Math.Min(1, double) 정수 overload로0.53이1로 반올림됐다. 실제 복원 x96.64056과 UTC 기대 x96.72는 일치했다. Math.Min(1.0, double)로 수정하고 같은 기록으로 재검증했다. 두 실패를 런타임 결함으로 보고하지 않는다.

기록된 정상 Windows trace를 통과시키고 camera/alpha/direction/clip/walkStartedMs 손상을 각각 거부함을 확인했다. 최종 CI38666625639 결과와 실행 증거는 검증 요약에서 확정한다.

## 판정 한계

400ms는 논리 이동 구간이다. 실제 행동 시작 간격에는 Windows timer·스케줄링·durable 저장 지연이 포함된다. 기계적 중간 위치·alpha·camera 검사와 대표 BMP 검토는 사용자 Windows11의 육안 품질·실제 OS 사건·장기 성능을 대신하지 않는다. 최종 제품 전체 완료로 확대하지 않는다.

## 최종 판정

CI38666625639의 core/windows 모두 success. Windows Server2025/MSVC19.51 native10/10(17.44초), 고정31시나리오·2,362합성frame·2,055제출·alpha255·중단 복원과 기존 생활/전투/루트 EXE 실행·복원 검사 통과. Linux CI sanitizer9/9(83.05초), Python26/26(9.384초) 통과. 실제 행동 시작 간격은400~413ms, 중앙값412ms였고 논리 구간은400ms다. 원작 frame BMP 네 장을 검토했고 네 변/모서리의 투명 영역은 관찰되지 않았다. 원본CSV·선택PNG·중단 전후 검사 결과와 SHA256은 검증 요약에 보존했다.
