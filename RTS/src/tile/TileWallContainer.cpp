#include "stdafx.h"
#include "TileWallContainer.h"

void TileWallContainer::init(const TileSpatialGrid* tileSpatialGrid) {
    mTileSpatialGrid = tileSpatialGrid;
    assert(mTileSpatialGrid);
    assert(mTileSpatialGrid->getNumTiles());
}

void TileWallContainer::resizeForCopy(size_t numTiles) {
    mWalls.resize(numTiles * 2);
}

void TileWallContainer::copyFrom(const TileWallContainer& other) {
    mTileSpatialGrid = other.mTileSpatialGrid;
    if (other.mWalls.size()) {
        if (mWalls.empty()) allocate();
        assert(mWalls.size() == other.mWalls.size());
        memcpy(mWalls.data(), other.mWalls.data(), mWalls.size() * sizeof(TileWall));
    }
    else {
        mWalls.clear();
    }
}

void TileWallContainer::destroy() {
    std::vector<TileWall>().swap(mWalls);
    mTileSpatialGrid = nullptr;
}

TileWall TileWallContainer::getWallAtTile(TileIndex tileIndex, Cartesian cartesian) const {
    if (mWalls.empty()) return TileWall();
    switch (cartesian) {
        case Cartesian::SOUTH:
            return getSouthWallAtTile(tileIndex);
        case Cartesian::WEST:
            return getWestWallAtTile(tileIndex);
        case Cartesian::EAST:
            return getEastWallAtTile(tileIndex);
        case Cartesian::NORTH:
            return getNorthWallAtTile(tileIndex);
        default:
            assert(false);
            break;
    }
    return TileWall();
}

TileWall TileWallContainer::getSouthWallAtTile(TileIndex tileIndex) const {
    if (mWalls.empty()) return TileWall();
    return mWalls[tileIndex];
}

TileWall TileWallContainer::getWestWallAtTile(TileIndex tileIndex) const {
    if (mWalls.empty()) return TileWall();
    return mWalls[mTileSpatialGrid->getNumTiles() + tileIndex];
}

TileWall TileWallContainer::getEastWallAtTile(TileIndex tileIndex) const {
    if (mWalls.empty()) return TileWall();
    if ((tileIndex % mTileSpatialGrid->getDims().x) >= (mTileSpatialGrid->getDims().x - 1)) {
        return TileWall();
    }
    return mWalls[mTileSpatialGrid->getNumTiles() + tileIndex + 1];
}

TileWall TileWallContainer::getNorthWallAtTile(TileIndex tileIndex) const {
    if (mWalls.empty()) return TileWall();
    const i32 floorStride = mTileSpatialGrid->getFloorStride();
    if (((tileIndex % floorStride) / mTileSpatialGrid->getDims().x) >= (mTileSpatialGrid->getDims().y - 1)) {
        return TileWall();
    }
    return mWalls[tileIndex + mTileSpatialGrid->getDims().x];
}

void TileWallContainer::getSouthAndWestWallsAtTile(TileWall outWalls[2], TileIndex tileIndex) const {
    outWalls[0] = getSouthWallAtTile(tileIndex);
    outWalls[1] = getWestWallAtTile(tileIndex);
}

void TileWallContainer::getWallsAtTile(TileWalls& outWalls, TileIndex tileIndex) const {
    outWalls.south = getSouthWallAtTile(tileIndex);
    outWalls.west = getWestWallAtTile(tileIndex);
    outWalls.east = getEastWallAtTile(tileIndex);
    outWalls.north = getNorthWallAtTile(tileIndex);
}

void TileWallContainer::setWallsAtTile(TileIndex index, TileWall walls[4]) {
    setSouthWallAtTile(index, walls[e_cast(Cartesian::SOUTH)]);
    setWestWallAtTile(index, walls[e_cast(Cartesian::WEST)]);
    setEastWallAtTile(index, walls[e_cast(Cartesian::EAST)]);
    setNorthWallAtTile(index, walls[e_cast(Cartesian::NORTH)]);
}

void TileWallContainer::setSouthWallAtTile(TileIndex tileIndex, TileWall wall) {
    if (mWalls.empty()) allocate();
    mWalls[tileIndex] = wall;
}

void TileWallContainer::setWestWallAtTile(TileIndex tileIndex, TileWall wall) {
    if (mWalls.empty()) allocate();
    mWalls[mTileSpatialGrid->getNumTiles() + tileIndex] = wall;
}

void TileWallContainer::setEastWallAtTile(TileIndex tileIndex, TileWall wall) {
    if (mWalls.empty()) allocate();
    if ((tileIndex % mTileSpatialGrid->getDims().x) >= (mTileSpatialGrid->getDims().x - 1)) {
        return;
    }
    mWalls[mTileSpatialGrid->getNumTiles() + tileIndex + 1] = wall;
}

void TileWallContainer::setNorthWallAtTile(TileIndex tileIndex, TileWall wall) {
    if (mWalls.empty()) allocate();
    const i32 floorStride = mTileSpatialGrid->getFloorStride();
    if (((tileIndex % floorStride) / mTileSpatialGrid->getDims().x) >= mTileSpatialGrid->getDims().y - 1) {
        return;
    }
    mWalls[tileIndex + mTileSpatialGrid->getDims().x] = wall;
}

void TileWallContainer::setWallAtTile(TileIndex tileIndex, TileWall wall, Cartesian cartesian) {
    switch (cartesian) {
        case Cartesian::SOUTH:
            setSouthWallAtTile(tileIndex, wall);
            break;
        case Cartesian::WEST:
            setWestWallAtTile(tileIndex, wall);
            break;
        case Cartesian::EAST:
            setEastWallAtTile(tileIndex, wall);
            break;
        case Cartesian::NORTH:
            setNorthWallAtTile(tileIndex, wall);
            break;
        default:
            assert(false);
            break;
    }
}

void TileWallContainer::allocate() {
    mWalls.resize(mTileSpatialGrid->getNumTiles() * 2);
}
