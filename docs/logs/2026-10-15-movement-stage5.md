# 실행물·문서 정리 5단계

루트 EXE는 이번 선택형 trace가 포함된 MinGW14 x64 Release이며 windows-runtime.json의source/assets hash와 일치한다. code/assets는 성공한 CI38666625639의20974b4와 동일하다. ROM/rom-files.zip은 불변이며 새 배포 ZIP을 만들지 않았다.

상태 문서의 최신 요약, README, 인수인계, 임시 문제 문서, Windows 실행·직접 수정 안내를 최신 판정으로 연결했다. MOVEMENT_VALIDATION.md에 로컬/Windows 재현 명령·관찰 계약·프레임 기록·남은 범위를 정리했다. 임시 문서는 실제 사용자 Windows11의 체감 문제 전체 확인이 남아 있어 보존한다.

검증: 루트 EXE/source/assets hash 일치, 검증 요약의38개 증거 파일을 작업 폴더/커밋 blob 양쪽SHA256과 대조, 문서 내부 파일 링크·diff whitespace 통과. docs-only 정리이므로 제품 회귀를 반복하지 않는다. 최종 커밋을 work로 push하고 원격/로컬 SHA 일치를 확인한다. 원격 main은 없고 origin/HEAD=work다.

단계 커밋:1=83b57a5,2=a75d13d,3=affde50,4=11c5d3e. 4단계의 CI 연결/판정 보완은e58bc6e·701215c·20974b4다. 다음 독립 범위는 사용자 Windows11 육안·실제 OS 사건·장기 성능 등이다.
