#include "stdafx.h"
#include "TileMutationDef.h"

SERIALIZABLE_ENUM_SAME_NAME(TileMutationType,
    ENUM_FIELD_SIMPLE(TileMutationType, BCorrupt),
    ENUM_FIELD_SIMPLE(TileMutationType, BPurify),
    ENUM_FIELD_SIMPLE(TileMutationType, CCorrupt),
    ENUM_FIELD_SIMPLE(TileMutationType, CPurify),
    ENUM_FIELD_SIMPLE(TileMutationType, Grow),
    ENUM_FIELD_SIMPLE(TileMutationType, Decay)
);
static_assert(e_count(TileMutationType) == 6);

SERIALIZABLE_IMGUI_CONTROLLED(TileMutationDef,
    make_field(o.type, "type"sv),
    make_field(o.target, "target"sv)
);