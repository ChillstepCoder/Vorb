#include "stdafx.h"
#include "TileTransformationDef.h"

SERIALIZABLE_ENUM_SAME_NAME(TileTransformationType,
    ENUM_FIELD_SIMPLE(TileTransformationType, BCorrupt),
    ENUM_FIELD_SIMPLE(TileTransformationType, BPurify),
    ENUM_FIELD_SIMPLE(TileTransformationType, CCorrupt),
    ENUM_FIELD_SIMPLE(TileTransformationType, CPurify),
    ENUM_FIELD_SIMPLE(TileTransformationType, Grow),
    ENUM_FIELD_SIMPLE(TileTransformationType, Decay)
);
static_assert(e_count(TileTransformationType) == 6);

SERIALIZABLE_IMGUI_CONTROLLED(TileTransformationDef,
    make_field(o.type, "type"sv),
    make_field(o.target, "target"sv)
);