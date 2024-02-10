#include "stdafx.h"
#include "WorldSaveContext.h"

#include "world/World.h"

WorldSaveContext::WorldSaveContext(World& world) :
    mWorld(world),
    mHeightHeader(mWorld.getWidthTiles()),
    mBiomeFiles(mWorld.getWidthTiles()) {
    LOG_INFO("  Height region count: {} - Patches Per Region - {}", mHeightHeader.getRegionCount(), mHeightHeader.getPatchesPerRegion());
    LOG_INFO("  Biome region count: {} - Patches Per Region - {}", mBiomeFiles.getRegionCount(), mBiomeFiles.getPatchesPerRegion());
}

WorldSaveContext::~WorldSaveContext() = default;
