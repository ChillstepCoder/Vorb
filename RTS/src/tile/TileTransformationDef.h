#pragma once

enum class TileTransformationType : ui8 {
    BCorrupt,
    BPurify,
    CCorrupt,
    CPurify,
    Grow,
    Decay,
    COUNT
};
SERIALIZABLE_ENUM_DECL(TileTransformationType);

struct TileTransformationDef {
    TileTransformationType type = TileTransformationType::BCorrupt;
    TileAssetRef target;
};
SERIALIZABLE_IMGUI_CONTROLLED_DECL(TileTransformationDef);