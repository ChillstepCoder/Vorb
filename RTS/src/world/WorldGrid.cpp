#include "stdafx.h"
#include "WorldGrid.h"

WorldGrid::WorldGrid() {
    for (ui32 i = 0; i < numChunks(); ++i) {
        mChunks[i].init(ChunkID(i), *this);
    }
    for (ui32 i = 0; i < numRegions(); ++i) {
        mRegions[i].init(RegionID(i));
    }
}
