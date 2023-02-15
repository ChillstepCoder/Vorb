#include "stdafx.h"

#include "city/Building.h"
#include "StructureManager.h"

#include "world/IWorld.h"

StructureManager::StructureManager() {

}

Structure* StructureManager::makeNewStructure(StructureType type, const i32AABB3& aabb, ui32 floorHeight) {
    assert(IS_GAME_THREAD());
    i32v3 tileDims = aabb.dims;
    std::unique_ptr<Structure> newStructure;
    switch (type) {
        case StructureType::Building: {
            newStructure = std::make_unique<Building>();
            newStructure->mType = StructureType::Building;
            newStructure->mTileContainer = TileContainerRepository::getNewTileContainer(aabb.pos, tileDims, floorHeight, (Building*)newStructure.get());
            break;
        }
        default:
            assert(false && "Invalid structure type");
    }
    newStructure->mAABB = aabb;
    assert((tileDims.z % floorHeight) == 0);
    tileDims.z /= floorHeight;
    newStructure->mTileContainer->allocateData();
    newStructure->mId = (StructureID)mStructures.size();
    // TODO: This structure ID needs a lookup

    BBox newBox(BoxPoint(aabb.x, aabb.y), BoxPoint(aabb.x + aabb.width, aabb.y + aabb.depth));
    mSpatialLookup.insert(std::make_pair(newBox, newStructure->mId));

    Structure* rv = newStructure.get();
    mStructures.emplace_back(std::move(newStructure));

    // Set up struct pointers on the chunk grid
    i32v2 worldXY;
    for (worldXY.x = aabb.x; worldXY.x < aabb.x + aabb.width; ++worldXY.x) {
        for (worldXY.y = aabb.y; worldXY.y < aabb.y + aabb.depth; ++worldXY.y) {
            Chunk& chunk = sWorld->getChunkAtPosition(worldXY);
            assert(chunk.isDataReady());
            const i32v2 xyOffset(worldXY.x - chunk.getChunkID().getWorldPosInt().x, worldXY.y - chunk.getChunkID().getWorldPosInt().y);
            chunk.setStructureAt(chunk.getTileContainer()->getTileIndexFromXYZOffset(xyOffset.x, xyOffset.y, 0), rv);
        }
    }

    return rv;
}
