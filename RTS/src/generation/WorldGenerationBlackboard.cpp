#include "stdafx.h"
#include "WorldGenerationBlackboard.h"

WorldGenerationBlackboard::WorldGenerationBlackboard(ui32 widthPatches) {
    mPeakPositions.resize(SQ(widthPatches), i32v2(-1));
}
