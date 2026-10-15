# work 브랜치 통합

2026-10-15 사용자 요청: 최신 작업이 work에 있는지 확인하고 work를 GitHub 기본 브랜치로 바꾸며 main을 삭제한다.

## 확인과 통합

- 원격 main: `468afc49dd98efe0a5fb8fd3e6c44e0982d1cfca`, work: `ca3c90af9ad73542a0597c5cf33ab0b101600344`.
- 분기 이후 main 고유1/work 고유42커밋. main 고유 커밋은 직접 수정 안내서 게시이며 안내서와 게시 로그는 work와 bytes가 같다.
- main과 work를 병합하고 충돌한 README/PROJECT_STATE/DECISIONS는 최신 work 내용을 보존했다. 두 부모 이력을 모두 유지한다.
- 개발·조회 기준을 AGENTS/README에 명시했다. 최초 로컬 work가 main을 가리키며 fetch도 main만 가져오던 설정을 전체 브랜치 fetch와 origin/work 추적으로 수정한다.
- 병합된 제품 코드·자산·EXE·워크플로는 기존 원격 work와 동일하다. diff 검사와 기존 EXE/source/assets hash 검증을 수행하며 문서/브랜치 정리로 제품 실행 검사를 반복하지 않는다.

## 원격 반영과 남은 설정

통합 커밋을 원격 work/main 모두에 fast-forward 반영한다. 그러면 기본 main 조회도 최신 코드를 반환한다. GitHub 기본 브랜치 work 변경과 main 삭제는 아직 미완료다. 연결된 GitHub 도구에는 기본 브랜치 설정 변경 기능이 없고, 실행기의 GitHub API 호출은 Forbidden으로 차단되어 있다. GitHub 연결 검색에서도 현재 설치된 동일 연결만 확인했다.

저장소 Settings → General → Default branch에서 work로 바꾼 뒤 main을 삭제할 수 있다. 기본 브랜치를 바꾸기 전에는 main을 삭제할 수 없다. 이후 원격 HEAD=work/main 부재를 확인하고 이 문서의 미완료 상태를 갱신한다.
