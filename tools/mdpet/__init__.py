"""오프라인 ROM 추출·선택 자산 변환. 런타임에서는 사용하지 않는다."""

SCHEMA_VERSION = 1
ASSET_VERSION = 1
SKYTEMPLE_COMMIT = "5ed359a2560a1f04666b9ea02b71649b89cb16d7"
REQUIRED = {"Walk": 0, "Idle": 7, "Eat": 15, "Sleep": 5, "Wake": 14, "Pose": 17}
OPTIONAL = {"EventSleep": 13, "Laying": 27}
