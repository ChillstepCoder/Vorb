#pragma once

enum class TileMutationType : ui8 {
    BCorrupt,
    BPurify,
    CCorrupt,
    CPurify,
    Grow,
    Decay,
    COUNT
};
SERIALIZABLE_ENUM_DECL(TileMutationType);

struct TileMutationDef {
    TileMutationType type = TileMutationType::BCorrupt;
    TileAssetRef target;
};
SERIALIZABLE_IMGUI_CONTROLLED_DECL(TileMutationDef);