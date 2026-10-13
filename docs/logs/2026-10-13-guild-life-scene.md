# 길드 공동 식사·주인공 방 연결

사용자가 원본 길드 주인공 방·식당·길드원 공동 식사 애니메이션을 중요한 필수 요구로 확정했다. 기존 구현은 배경+주인공만 표시하고 침상 좌표 대신 화면 중심을 사용하고 있어 보완했다.

원본 ROM의 SSA/SSB와 overlay11 SETANIMATION_TABLE을 직접 확인했다. G01P06B/c00a0601은 주인공·동료·길드원10명·object107이며, G01P07A/c00a0201은 주인공·동료의 실제 침상과 Right2/Left6을 지정한다. 식당 배경 crop(180,112), 침실 crop(92,56)에 원본 좌표를 정규화했다.

SSB strict 파서는 원본의 unknown byte144에서 실패했다. 대사 수정 없이 byte-preserving 문자열 처리로 opcode만 읽고 실제 대사 디코딩은 unresolved로 남겼다. 원본 SetAnimation43/42/5의 실제 테이블 word0x080E/080D/0800을 확인했다. 특히 푸크린 code5는 Sleep5가 아니라 Walk0 loop였으며 추측하지 않고 원본 테이블로 해석했다.

길드원은 각각의 실제 MD/sprite/WAN으로 최소Idle+지정 식사 동작을 추출했다. 원본 WAN→merged→atlas516개/원본996frame 대조pass. GROUND/g01p06b1.wan은 character WAN이 아니므로 일반 WAN parser로 frame·anchor를 추출했다. 효과 ID642/643/624를 pack slot으로 추측하지 않았다.

기존 MDPA/WIC/Animator/Surface와 atomic 생활 transaction을 재사용한다. 길드원 texture는 집 장면에서만 lazy load하고 캐시한다. 식사는8초 동안 개체별 loop를 유지하며 consume once, 취침은 실제 침상에서EventSleep13, 기상은 두 개체의Wake14 완료를 기다린다. Laying27 원본 지정과 별도 식사 효과는 근거·미완료 범위로 기록한다.

Linux4CTests pass(17.48s), Python23tests pass, Windows MinGW compile/link pass. 길드원10명 모두 실제frame 변화·식당13layers·침실2명·정확한 좌표/방향, 기존12개생활/전투 시나리오·12,031복원을 검사했다. 단일Eat clip 완료로 공동식사를 조기 종료하지 않는 검사도 추가했다. Windows 실행 결과는 후속 기록에 추가한다.

미리보기는 실제 MDPA를 native inspector로 읽어 만든 `local/guild-preview/guild-dining.gif`, `player-room-sleep.gif`다. 원작 공동 식사 효과·음악·대사·Turn2Direction의 전체 재현, 실제Windows11·장기 성능은 pending이다.
