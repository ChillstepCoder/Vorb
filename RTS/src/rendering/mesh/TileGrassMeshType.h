#pragma once

enum class TileGrassMeshType : ui8 {
    DEFAULT,
    PLANE,
    BILLBOARD,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TileGrassMeshType,
    pair{ TileGrassMeshType::DEFAULT, "default"sv},
    pair{ TileGrassMeshType::PLANE, "plane"sv},
    pair{ TileGrassMeshType::BILLBOARD, "billboard"sv}
)
static_assert(e_cast(TileGrassMeshType::COUNT) == 3, "Update yml def");