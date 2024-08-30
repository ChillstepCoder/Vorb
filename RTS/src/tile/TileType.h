#pragma once

enum class TileType : ui8 {
    Default,
    Flora,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TileType,
    ENUM_FIELD_SIMPLE(TileType, Default),
    ENUM_FIELD_SIMPLE(TileType, Flora),
);