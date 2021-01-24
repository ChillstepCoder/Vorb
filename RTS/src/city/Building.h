#pragma once

// TODO: Can we optimize passing this around so theres no copies? Does it matter...? no
struct Building {
    // Building bounds are a series of corner segments
    // Each pair is an X,Y offset pair representing a wall "corner" so this must be even in length
    std::vector<i16v2> mWallSegmentOffsets;
    ui32v2 mBottomLeftWorldPos;
};

struct PlannedBuilding {
    Building mBuilding;
};