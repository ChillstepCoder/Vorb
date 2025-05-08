#pragma once

// Height can only qualify as a peak if it is greater than this
constexpr f32 MIN_GENERATION_PEAK_HEIGHT = 100.0f;
constexpr i32 RIVER_CARVE_LOCAL_GROUP_SIZE = 32; // Match GLSL / 2 (Each compute thread processes 2x2 verts)

struct RiverPath {
    std::vector<f32v2> splinePath;
    std::vector<i16v2> visited;
    // Compute will batch all vertices into local groups
    UnorderedFlatMap<i32v2 /*vertexPosCorner*/, std::vector<f32v4> /*segments*/> affectedLocalGroups;
    i16v2 startPoint;
    bool isValid = false;
};

class WorldGenerationBlackboard
{
public:
    WorldGenerationBlackboard(ui32 widthPatches);

    // Mountain peaks for river generation and other stuff
    std::vector<i32v2 /*worldPos*/> mPeakPositions; // Index is height patch index
    std::vector<RiverPath> mRiverPaths;
    bool mRiverSplinesGenerated = false;
};
