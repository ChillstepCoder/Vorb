#include "stdafx.h"
#include "MutationDef.h"

SERIALIZABLE_ENUM_SAME_NAME(MutationType,
    ENUM_FIELD_SIMPLE(MutationType, BCorrupt),
    ENUM_FIELD_SIMPLE(MutationType, BPurify),
    ENUM_FIELD_SIMPLE(MutationType, CCorrupt),
    ENUM_FIELD_SIMPLE(MutationType, CPurify),
    ENUM_FIELD_SIMPLE(MutationType, Grow),
    ENUM_FIELD_SIMPLE(MutationType, Decay)
);
static_assert(e_count(MutationType) == 6);

SERIALIZABLE_IMGUI_CONTROLLED(MutationDef,
    make_field(o.type, "type"sv),
    make_field(o.target, "target"sv)
);