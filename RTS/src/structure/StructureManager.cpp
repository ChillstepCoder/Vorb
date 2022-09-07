#include "stdafx.h"

#include "city/Building.h"
#include "StructureManager.h"

#include "world/World.h"

StructureManager::StructureManager(World& world) : mWorld(world) {

}

Structure* StructureManager::makeNewStructure(StructureType type, const i32AABB3& aabb, ui32 floorHeight) {
    assert(IS_MAIN_THREAD());
    std::unique_ptr<Structure> newStructure;
    switch (type) {
        case StructureType::Building: {
            newStructure = std::make_unique<Building>();
            newStructure->mType = StructureType::Building;
            break;
        }
        default:
            assert(false && "Invalid structure type");
    }
    newStructure->mAABB = aabb;
    ui32v3 tileDims = aabb.dims;
    assert((tileDims.z % floorHeight) == 0);
    tileDims.z /= floorHeight;
    newStructure->mTileContainer = TileContainerRepository::getNewTileContainer(aabb.pos, tileDims, floorHeight, false /*isTerrain*/);
    newStructure->mTileContainer->allocateData();
    newStructure->mId = mStructures.size();
    // TODO: This structure ID needs a lookup

    BBox newBox(BoxPoint(aabb.x, aabb.y), BoxPoint(aabb.x + aabb.width, aabb.y + aabb.depth));
    mSpatialLookup.insert(std::make_pair(newBox, newStructure->mId));

    Structure* rv = newStructure.get();
    mStructures.emplace_back(std::move(newStructure));

    // Set up struct pointers on the chunk grid
    ui32v2 worldXY;
    for (worldXY.x = aabb.x; worldXY.x < aabb.x + aabb.width; ++worldXY.x) {
        for (worldXY.y = aabb.y; worldXY.y < aabb.y + aabb.depth; ++worldXY.y) {
            Chunk& chunk = mWorld.getChunkAtPosition(worldXY);
            assert(chunk.isDataReady());
            const ui32v2 xyOffset(worldXY.x - chunk.getChunkID().getWorldPosInt().x, worldXY.y - chunk.getChunkID().getWorldPosInt().y);
            chunk.setStructureAt(chunk.getTileContainer()->getTileIndexFromXYZOffset(xyOffset.x, xyOffset.y, 0), rv);
            // Nav is dirty since structure will block
            chunk.getTileContainer()->setDirtyNav(true);
        }
    }

    return rv;
}
