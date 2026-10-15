# work 브랜치 통합

2026-10-15 사용자 요청: 최신 작업이 work에 있는지 확인하고 work를 GitHub 기본 브랜치로 바꾸며 main을 삭제한다.

## 확인과 통합

- 원격 main: `468afc49dd98efe0a5fb8fd3e6c44e0982d1cfca`, work: `ca3c90af9ad73542a0597c5cf33ab0b101600344`.
- 분기 이후 main 고유1/work 고유42커밋. main 고유 커밋은 직접 수정 안내서 게시이며 안내서와 게시 로그는 work와 bytes가 같다.
- main과 work를 병합하고 충돌한 README/PROJECT_STATE/DECISIONS는 최신 work 내용을 보존했다. 두 부모 이력을 모두 유지한다.
- 개발·조회 기준을 AGENTS/README에 명시했다. 최초 로컬 work가 main을 가리키며 fetch도 main만 가져오던 설정을 전체 브랜치 fetch와 origin/work 추적으로 수정한다.
- 병합된 제품 코드·자산·EXE·워크플로는 기존 원격 work와 동일하다. diff 검사와 기존 EXE/source/assets hash 검증을 수행하며 문서/브랜치 정리로 제품 실행 검사를 반복하지 않는다.

## 원격 반영과 설정 완료

통합 커밋 `a7ca6aab974a181cf8766086553b308ede26a3c6`을 `git push --atomic`으로 원격 work/main 모두에 fast-forward 반영했다. `git ls-remote`에서 양쪽 SHA 일치를 확인해 기본 main 조회도 최신 코드를 반환한다. 통합 커밋은 기존 main/work 두 HEAD를 모두 부모로 포함한다. 이후 이 문서에 검증 결과를 기록하는 커밋도 두 브랜치에 동일하게 반영한다.

검증: 기존 work 대비 변경은 AGENTS/README/PROJECT_STATE/DECISIONS/이 로그5개 문서뿐이다. `git diff --cached --check origin/work`와 코드·자산·EXE·도구·시험·워크플로 동일성 검사 통과. `python tools/check_windows_runtime.py`가 Windows x64 runtime/source/assets hashes match로 통과했다. 기존 Windows 검사 로그의 CRLF/공백은 수정하지 않았다. 로컬 work는 origin/work를 추적하며 전체 브랜치 fetch 설정을 확인했고 작업 트리는 깨끗했다.

최초 통합 직후에는 GitHub 기본 브랜치 work 변경과 main 삭제가 미완료였다. 연결된 GitHub 도구에는 기본 브랜치 설정 변경 기능이 없고, 실행기의 GitHub API 호출은 Forbidden으로 차단되어 있어 사용자가 저장소 설정에서 직접 기본 브랜치를 work로 변경했다.

사용자의 변경 완료 응답 후 저장소 메타데이터 default_branch=work와 `git ls-remote --symref`의 HEAD=work를 확인했다. fetch 후 main/work 모두 `0ebca77c04b77f4ffc024f3821f4432a2b9cee38`, 고유 커밋0/0임을 재검사했다. 해당 main SHA에 대한 lease를 지정해 원격 main을 삭제했고 성공했다. prune과 remote set-head 후 원격 브랜치는 work 하나이며 origin/HEAD=work임을 확인했다. main의 모든 이력은 통합 커밋을 통해 work에 보존된다. 이 완료 기록은 work에만 반영한다.

## main 필요성 재확인

사용자의 삭제 전 재확인 요청은 main 삭제 성공 이후 도착했다. 삭제 직전 SHA 동일성·main 이력 포함 여부를 확인했고, 추가로 다음을 재확인했다.

- 초기 main `468afc4`와 삭제 직전 main `0ebca77`은 모두 origin/work의 조상이다. main에만 남은 커밋은 없다.
- 열린 PR은0개다. `.github/workflows/a2.yml`은 push/pull_request에 브랜치 제한이 없고 checkout ref도 main으로 고정하지 않는다.
- 추적 파일308개에서 이 저장소의 main URL·origin/main·refs/heads/main·명시적인 main checkout 명령을 검사했고 일치 항목은 없다. QUICK_EDIT_GUIDE의 오래된 main 게시 설명은 현재 기본 work 설명으로 수정했다.
- 최초 최신 work `ca3c90a`와 비교해 제품 소스·자산·EXE·실행 manifest·도구·시험·CI 워크플로는 동일하다. Windows runtime/source/assets hash 검증도 재통과했다.
- 확인한 저장소와 GitHub PR/CI 구성에는 main 유지가 필요한 의존성이 없다. 삭제 전 HEAD `0ebca77c04b77f4ffc024f3821f4432a2b9cee38`는 work 이력 안에 보존되어 필요하면 해당 SHA로 main을 복원할 수 있다.
