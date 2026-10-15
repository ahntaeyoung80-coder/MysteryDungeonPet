# 실행물·안내·재개 정리 완료

루트 EXE와 windows-runtime.json은 Windows CI가 검증한 구현605c300 그대로이며 source/assets hash 일치를 다시 확인했다. 새 원작 추출·자산 변경·배포 ZIP은 없다. 원본 ROM/ZIP hash는 유지한다.

README·Windows 실행 안내·직접 수정 안내·현재 상태·인수인계·임시 문제·설계 결정을 최신 대각선 반복 수정/자동 귀가/수동 Reset·MDES6 기준으로 갱신했다. 이전 미재현 기록보다 후속 재현을 우선하도록 연결했고 검증 원본·hash와 단계별 커밋을 기록했다. 문서/증거만 변경했으므로 통과한 전체 검사를 다시 실행하지 않는다.

재개는 origin/work fetch/HEAD 비교 후 새 보고의 snapshot/`.reset-backup` 보존부터다. 이번 버그/Reset 범위 완료와 실제 사용자 Windows11·OS 사건·장기 성능/전체 콘텐츠 미검증을 구분한다. 최종 문서 커밋을 work에 push하고 로컬/원격 SHA 및 clean 상태를 확인한다.
