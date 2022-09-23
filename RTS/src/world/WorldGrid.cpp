#include "stdafx.h"
#include "WorldGrid.h"



WorldGrid::WorldGrid(World& world) : HeightmapGrid(world), mChunkGrid(*this) {
   
}

void WorldGrid::tick(const f32v2& loadCenter) {
    mLoadCenter = loadCenter;
    mChunkGrid.tick(mLoadCenter);
}


HeightmapPatch::~HeightmapPatch() {
    delete[] mHeightData;
}
