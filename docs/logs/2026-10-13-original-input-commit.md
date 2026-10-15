# 원작 입력 전체 Git 보관

2026-10-13 15:36 KST. 사용자 요청으로 원본 ROM까지 포함한 입력 전체를 커밋한다. 기존 제외 규칙은 명시된 입력에 대해 변경했다. 예상10~20분, 단계별 기록을 아래에 남긴다.

## 준비·검사 완료

- 원본 `assets/original/source/user-supplied.nds`: 91,394,048bytes, SHA-256 `6dc1ba5320168b1ce6fdb0f4fc658c67e4c579623b306d64edbedff1ec5b3faf`. 기존 원본과 정확히 일치.
- 모든 내부 파일8644개+ARM9/ARM7/overlay tables/postdata5개를 `source/rom-files.zip`에 보존. 49,668,567bytes, SHA-256 `cc9d113ea4b438d8b8895a30c3d47863dc99cdb1e1381ce8c029aeeb8d748b54`. CRC 전체 및8649항목 개별SHA 일치 pass.
- source/inventory.json에 파일ID·원본경로·크기·해시 기록. 이름 없는 ROM 파일은 fileId 기반 _unnamed 경로로 보존.
- 이전 A-1 전체 감사본 ZIP587,906bytes와 전송 해시도 보존. 실제2종 MDPA/PNG/CREDITS/runtime-index 추가.
- C++17 package_inspect 빌드 후 ROM MDPA2개/clip16/348frames를 A-1 변환값과 대조. rect/anchor/shadow/markers/ticks/rawFlags/offsets/metadata 모두 pass. Windows 실행·원작 walltime은 미검증 유지.
- 사용자 범위는 입력 전체이므로 대표2종만 저장하는 방식을 대체했다. 단, 전체8644파일의 디코드나 모든 종·폼의 변환 완료를 주장하지 않는다.
- .gitignore의 지정 원본 경로만 예외. local/·인증정보 제외 유지. pipeline/decisions/current state/사용법/인수인계 갱신.

## 재개 지점

다른 세션은 최신 work를 받아 assets/original/README.md를 읽고 A-3을 계속한다. 실제 재생은 runtime/asset-0/asset.mdpa 또는 asset-1/asset.mdpa. 전체 원본 재추출은 source/user-supplied.nds. 추가 사용자 업로드 불필요.

## 원격 반영

이 기록과 전체 입력을 함께 커밋/push하고 원격 SHA를 대조한다. 결과는 후속 기록에 남긴다.

## 2026-10-13 15:38 KST 원격 반영 완료

- 입력 전체 커밋 `84582b9c340616d568c9610dcb11311a5d864471`을 origin/work에 push 성공. git ls-remote SHA와 로컬 HEAD 일치 확인.
- Git에 실제 저장된 ROM/전체 파일 ZIP의 blob을 다시 읽어 SHA-256을 검사했고 원본/아카이브 인벤토리와 일치.
- GitHub는 ROM 87.16MB에 대해50MB권장 크기 경고를 출력했으나 업로드를 승인했다. 오류나 누락 없음. Git LFS 없이 일반 Git에 파일 bytes 저장됨.
- 이후 세션은 원본 재업로드 없이 assets/original/을 사용한다. 전체 입력 보관 완료, 전체 종·폼 변환 지원 감사는 별개.
