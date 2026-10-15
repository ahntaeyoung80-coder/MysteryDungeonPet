# MDPA v1 런타임 직렬화

`tools/package_format.py`가 쓰고 `src/core/package.cpp`가 읽는다. A-1 audit `manifest.json`을 `tools/compile_runtime_package.py`로 직렬화하는 한 의미 계약이다. `.mdpa`는 원작 ROM이나 catalogue가 아니다. 한 파일은 한 asset과 한 PNG atlas를 닫힌 집합으로 참조한다.

## 기본 규칙

모든 숫자는 little-endian, u32 또는 2의 보수 i32다. 문자열은 `u32 UTF8바이트수 + UTF8바이트`, NUL/잘못된 UTF8을 거부한다. 배열은 `u32 count + 레코드`로 길이를 제한하며 매 필드를 읽기 전에 남은 바이트를 검사한다. 마지막 필드 뒤의 데이터도 거부한다. 임의 struct memcpy·포인터·OS handle·절대 로컬 경로를 저장하지 않는다.

## 필드 순서

| 레코드 | 순서와 형식 |
| --- | --- |
| Header | u32 magic=0x4150444d(`MDPA`), schemaVersion=1, assetVersion, tickHz, atlasWidth, atlasHeight, pngBytes, sourceKind, shadowSize |
| Source | string atlasName(로컬 basename), pngSha256(소문자64hex), assetKey, provenance |
| Clips | u32 clipCount + Clip[] |
| Clip | u32 id, i32 sourceWanId, i32 aliasOf, u32 rawFlags, speedNumerator, speedDenominator; string name, sourceMetadata; u32 rowCount; Row[] |
| Row | u32 frameCount + Frame[] |
| Frame | Rect body; Point anchor, shadow; u32 durationTicks, pointCount; PointMarker[]; Rect shadow; Point cropOrigin; u32 rawWanFlag; Point rawOffset, rawShadow; u32 eventBits; string sourceMetadata |
| Rect | i32 x,y,width,height |
| Point | i32 x,y |
| PointMarker | u32 kind + Point |

sourceKind는 0=test, 1=rom, 2=public이다. 비ROM의 sourceWanId는 -1로 두어 원작 식별 성공을 만들지 않는다. aliasOf=-1은 비alias이며 그 외는 같은 asset의 selector id다. assetVersion은 선택 데이터의 버전이며 schemaVersion과 별개다.

PointMarker kind는 0=center, 1=head, 2=lhand, 3=rhand, 4=shadow다. 좌표 metadata이며 재생 이벤트와 별개다. eventBits는 bit0=rush, bit1=hit, bit2=return이다. Animator는 frame 진입 경계에서 해당 이벤트를 한 번 반환하며 event point에는 frame anchor를 붙인다. 몸체의 각 point는 Frame에서 별도로 참조한다.

shadowRect의 네 값이 0이면 그림이 없다. 그 외는 atlas 범위 안이어야 한다. A-1 출력의 SkyTemple RGB 크기 mask/흰 원점 marker를 그대로 보존하고 provenance로 의미를 추적한다. 표시용 RGBA 그림자로 취급하지 않는다. 원작 shadowSize·raw offset/shadow·cropOrigin·pixel hash를 유지한다.

provenance에는 A-1 source 전체, 원본 manifest hash, compiler hash, shadowEncoding과 시간 검증 상태가 들어간다. Clip sourceMetadata에는 sourceTiming/XML 정보, Frame sourceMetadata에는 rawWan/원본 RGBA와 shadow hash가 들어간다. 원본 추적용 UTF8 JSON 문자열이며 runtime은 내용을 해석하지 않고 보존한다. runtime의 동작은 같은 원본 필드에서 만든 binary 값을 사용한다.

## alias·시간

alias의 참조·순환은 전체 graph에서 검사한다. rowCount=0이면 대상 row를 공유한다. rowCount=1/8이면 자기 frame을 쓰면서 alias 참조와 자기 WAN tick/flag/event/count를 함께 유지한다. atlas를 공유하되 frame geometry가 다른 alias를 임의 해석하지 않는다. 1행 동작을 8방향으로 늘리지 않는다.

tick은 정수이며 시간은 monotonic microsecond다. `tickHz × speedNumerator/speedDenominator`로 검사 시간을 계산한다. 미검증 원작 속도는 sourceTiming에 null로 남긴다. 60Hz는 provisional 값이며 원작 wall time을 보장하지 않는다. finite는 마지막 frame을 유지하고 finish 이벤트를 한 번 반환한다. loop는 finish가 없으며 Stop/clip 전환으로 끝낸다.

clock이 크게 지연돼도 현재 frame만 표시한다. loop marker는 최대 이전/current 2cycle로 제한하고 그보다 긴 지연은 최신 cycle까지 이동해 skippedCycles를 반환한다. 과거 모든 frame·marker를 재생하지 않는다. 이 생략 규칙은 A-2 표시용이며 음식 소비 등 상태 변경은 후속 EventPlayer에서 once 처리한다.

## 거부 조건

manifest 2MiB, PNG 8MiB, decoded atlas 8MiB, clip 1..256, 총 frame 16,384, row 1/8, row frame 1..4096, point 16 이하, row total tick 3,600,000 이하로 제한한다. source 문자열 16KiB/frame source 4KiB 이하. atlas hash/byte size/dimensions를 WIC 전후로 대조하고 PBGRA만 합성한다. 최대 양수 크기끼리의 곱도 먼저 pixel 예산으로 검사해 byte 계산 overflow를 거부한다.

알 수 없는 schema, 빈 식별/출처, 범위 밖 rect, 0tick, 과대 count, 깨진 alias, 여분 데이터, 잘린 파일은 명시 오류다. 원작 필수 동작이 없으면 임의 대체하지 않는다. 변환은 기존 출력 덮어쓰기를 거부하고 reload는 검사된 candidate를 만든 뒤 현재 asset을 교체한다.
