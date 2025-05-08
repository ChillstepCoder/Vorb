#pragma once

enum class MutationType : ui8 {
    BCorrupt,
    BPurify,
    CCorrupt,
    CPurify,
    Grow,
    Decay,
    COUNT
};
SERIALIZABLE_ENUM_DECL(MutationType);

struct MutationDef {
    MutationType type = MutationType::BCorrupt;
    TileAssetRef target;
};
SERIALIZABLE_IMGUI_CONTROLLED_DECL(MutationDef);