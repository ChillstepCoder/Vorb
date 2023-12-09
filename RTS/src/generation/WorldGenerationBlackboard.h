#pragma once

// Height can only qualify as a peak if it is greater than this
constexpr f32 MIN_GENERATION_PEAK_HEIGHT = 100.0f;

struct RiverPath {
    std::vector<i16v2> points;
    std::vector<i16v2> visited;
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
    bool mRiversDone = false;
};

