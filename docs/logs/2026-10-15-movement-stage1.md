# 이동·카메라 안정화 1단계

원격 work=80470ba를 fetch/fast-forward했다. main이 없는 원격을 확인하고 fetch 범위를 전체 브랜치, work 추적을 origin/work로 바로잡았다. 미커밋 변경은 없었다.

루트 EXE/source/assets hash 일치. Linux GCC14.2 Release CTest7/7(15.87초) 통과. 기존 Windows GUI 이동 검사는 4초/1초 계측으로 대화·전투에 가려졌으므로 고정 snapshot과 프레임 단위 증거를 추가한다. 명세와 시나리오는 docs/MOVEMENT_STABILIZATION_SPEC.md다.

빌드 도구/고정 Python 의존성을 사용자 쓰기 영역에 설치했다. Windows 직접 실행기는 없으며 이후 CI로 네이티브 검사를 수행한다. 실제 Windows11 육안·OS 사건·장기 성능은 후속 범위다. 다음은 이동·방향 전환과 복원 검증이다.
