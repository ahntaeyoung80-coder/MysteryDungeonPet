# 실사용 품질 점검

PC 활동 기반 생활 정책을 추가하기 전에 실행 안정성·복원·자원 추이·자연 전투를 따로 확인한다. [2026-10-23 점검 결과](validation/2026-10-23-quality-check/README.md)를 기준으로 다음 변경을 선택한다.

## Windows 자료 수집

1. 실행 중인 앱을 우클릭 **Exit**로 닫고 최신 `work`를 pull한다.
2. 저장소 루트의 **QualityCheck.cmd**를 더블클릭한다. PowerShell 명령을 직접 입력할 필요가 없다.
3. 약30분 실행과15초 재실행을 기다린다. 정상 관측 중에는 앱을 직접 종료하지 않는다. 수집기는 기존에 실행 중인 앱을 종료하지 않는다.
4. 콘솔의 `결과 ZIP` 경로를 확인한다. 기본 폴더는 `%LOCALAPPDATA%\MysteryDungeonPet\quality-results\날짜-시각`이다. ZIP을 전달하면 분석할 수 있다. 오류가 나면 콘솔 문구와 이미 만들어진 결과 폴더도 보존한다.

기존 모험 저장과 분리된 `quality-state.mdes`를 사용한다. 점검 전 창 설정은 종료 때 복원한다. Windows 잠금/절전 동안에도 수집 시간이 흐르며, 복귀 후 기록이 일찍 끝나면 합격으로 처리하지 않는다. 기본 첫 관측은 잠금·절전 없이 진행하고 아래 수동 점검은 별도로 구분한다.

`tools/run_quality_check.ps1`은 Windows PowerShell5.1 이상을 대상으로 한다. 현재 환경에서는 PowerShell7 구문 검사와 설치 포함을 확인했으며 **수집기의 실제 Windows 실행은 미검증**이다. 실행 정책 우회는 이 로컬 스크립트를 실행하는 해당 프로세스에만 적용한다. 설치본에도 같은 CMD와 tools 스크립트를 포함한다.

수집 내용은 실행 파일 SHA256, OS/코어 수, 앱 보고서·초당 CPU/메모리/렌더 계수, 외부 프로세스 메모리/핸들, 이번 실행의 이벤트 로그, 재실행 전후 저장이다. 상세 프레임 trace는 성능 관측을 왜곡하므로 사용하지 않는다. 키 입력 내용·사용 중인 창 제목·스크린샷은 수집하지 않는다. `collected`는 자료 수집 완료이며 품질 검사 전체 합격이 아니다.

개발자가 더 긴 관측을 할 때는 CMD 인수 `-Seconds 7200`으로2시간 실행할 수 있다. 최대4시간까지 지원한다. 중간 강제 종료나 수집기 실패 시 결과 폴더를 지우지 않는다.

## 별도로 확인할 Windows 동작

| 항목 | 확인할 결과 | 남길 자료 |
|---|---|---|
| 기본30분 | 화면이 계속 갱신되고 다음 날 모험까지 진행 | 결과 ZIP |
| 재실행 | 날짜·재고 보존, 정상 아침 및 새 원정 | 전후 저장과 보고서 |
| 드래그/배율1·2·3 | 위치 이동, 대사2줄과 화자 초상화가 잘리지 않음 | 발생 시각·배율·화면 배율 |
| 잠금/화면 꺼짐 | 출력 정지·복귀, 시간/소비가 중복되지 않음 | 잠금·해제 시각 |
| 절전/복귀 | 종료·멈춤·단계 건너뛰기 없이 재개 | 절전·복귀 시각 |
| DPI/모니터 변경 | 창이 보이고 크기·클릭 영역·텍스트가 맞음 | 모니터/DPI 전후 값 |
|2시간 자원 | warmup 이후 메모리·핸들·GDI의 지속 증가 여부 | 장시간 ZIP |

각 수동 항목은 현재 `unverified`로 기록한다. 자동 수집기가 수행한 것처럼 표시하지 않는다. 기본30분 관측과 장시간/OS 동작을 한 번에 섞으면 원인 분리가 어려우므로 각각 다른 결과 폴더로 보존한다.

## 개발자 재현

```bash
cmake -S . -B build/quality-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/quality-release --target pet_quality_probe
build/quality-release/pet_quality_probe --output build/quality-routes
python tools/summarize_quality.py --profiles build/quality-routes/cases.jsonl --output build/quality-routes/report.json
python -W error::ResourceWarning -m unittest discover -s tests -p test_quality_summary.py
```

코어 측정은8시작 원정×동료4seed와 단독1seed, 총40건이다. 체력/열매/계단을 강제로 조작하지 않고 자연 진행한다.250ms 가상 시계와600ms 완료 통지를 사용하므로 실제 화면 재생 시간이나 모집단 승률로 읽지 않는다. `--helper-hp 2 --roots 17,35`는 보조 적 체력 가설만 측정 도구 내부에서 바꾸며 제품에는 반영하지 않는다.

네이티브 앱 metrics는 `python tools/summarize_quality.py --metrics 경로 --output 결과.json`으로 요약한다. Wine 자료는 반드시 `--wine`을 붙인다. CPU는 한 코어 기준이고, 일시 정지·5초 초과 관측 공백을 평균에서 제외한다. CPU1%·working/private30MB 잠정 목표는 실제 Windows 수치에만 판정하며,0으로만 나오는 미지원 계수는 통과가 아닌 측정 불가다.30분 수치는 장기 누수나24시간 안정성을 입증하지 않는다.
