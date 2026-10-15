# 저장 호환 fixture

- legacy-v1.mdes / legacy-v2.mdes: 개선 1단계 당시 MDES2 writer의 초기 저장. MDES1은 기본 gameplay 확장84bytes를 제거하고 CRC를 재계산한 동일 바탕 형식이다.
- legacy-v2-completed-npc.mdes: 기존 fa65927의 실제 writer로 생성. 완료된 우호 NPC의 이전 타일에 플레이어가 있는 유효한 과거 저장이다. 원래 제거됐던 NPC의 비표시·플레이어 위치·onceKey를 유지한다.
- legacy-v2-contact.mdes: 같은 이전 writer로 생성. NPC 타일에 이동했으나 접촉 처리가 아직 끝나지 않은 과거 저장이다. RNG·위치·미완료를 보존하고 NPC를 가장 가까운 비점유 바닥으로 옮겨 중첩을 해소한다.
- legacy-v1-contact.mdes: 위 contact의 기본 gameplay 확장84bytes 제거 및 CRC 재계산. 배우/저장 상태의 MDES1 읽기도 검증한다.

모든 파일은 테스트용 합성 dungeon/profile이며 원본 ROM bytes를 포함하지 않는다. assetVersion은 legacy다. 구버전 writer의 소스는 git의 fa65927에서 확보했으며 제작용 브랜치 전환·원본 파일 변경은 하지 않았다.
