#pragma once

// Enough for a full chunk of grass + padding
// TODO: How much do we really save doing this?
// Profile how often we use this...
constexpr unsigned MAX_QUAD_MESH_INDICES = CHUNK_SIZE * 8 * 8 * 6 + CHUNK_SIZE * 6;

typedef i32 SubmeshIndex;

enum class MeshWindType : ui8 {
    None,
    Grass,
    TreeTrunk,
    TreeLeaves,
    COUNT
};
SERIALIZABLE_ENUM_DECL(MeshWindType);

enum class MeshLODLevel : ui8 {
    Highest,
    Medium,
    Low,
    Lowest,
    COUNT,
    IMPOSTOR = COUNT,
    INVALID = 255
};

struct MeshLODDrawInfo {
    ui32 startIndex;
    ui32 indexCount;
};