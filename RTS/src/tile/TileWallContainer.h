#pragma once

#include "tile/TileSpatialGrid.h"

struct TileWall {
    TileID wallID = TILE_ID_NONE;
    bool isDoor = false; // TODO: Flags
    //ui8 health = UINT8_MAX; TODO: USE

    void clear() { wallID = TILE_ID_NONE; }
    bool isValid() const { return wallID != TILE_ID_NONE; }
    bool canNavThrough() const { return isDoor || wallID == TILE_ID_NONE; }
};

constexpr f32 WALL_THICKNESS = 0.15f;
constexpr f32 WALL_HALF_THICKNESS = WALL_THICKNESS / 2.0f;

struct TileWalls {
    TileWalls() : south(), west(), east(), north() {};

    union {
        TileWall walls[4];
        struct {
            TileWall south;
            TileWall west;
            TileWall east;
            TileWall north;
        };
    };
};

// TODO: Extract file
// Holds walls for a TileContainer. The +x and +y outermost edges cannot have walls
class TileWallContainer {
public:
    void init(const TileSpatialGrid* tileSpatialGrid) {
        mTileSpatialGrid = tileSpatialGrid;
        assert(mTileSpatialGrid);
        assert(mTileSpatialGrid->getNumTiles());
        mWalls.resize(mTileSpatialGrid->getNumTiles() * 2);
    }
    void resizeForCopy(size_t numTiles) {
        mWalls.resize(numTiles * 2);
    }
    void copyFrom(const TileWallContainer& other) {
        assert(mWalls.size() == other.mWalls.size());
        mTileSpatialGrid = other.mTileSpatialGrid;
        memcpy(mWalls.data(), other.mWalls.data(), mWalls.size() * sizeof(TileWall));
    }
    // TODO: Serialize
    void destroy() {
        std::vector<TileWall>().swap(mWalls);
        mTileSpatialGrid = nullptr;
    }
    TileWall getWallAtTile(TileIndex tileIndex, Cartesian cartesian) const {
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
    TileWall getSouthWallAtTile(TileIndex tileIndex) const {
        return mWalls[tileIndex];
    }
    TileWall getWestWallAtTile(TileIndex tileIndex) const {
        return mWalls[mTileSpatialGrid->getNumTiles() + tileIndex];
    }
    TileWall getEastWallAtTile(TileIndex tileIndex) const {
        if ((tileIndex % mTileSpatialGrid->getDims().x) >= (mTileSpatialGrid->getDims().x - 1)) {
            return TileWall();
        }
        return mWalls[mTileSpatialGrid->getNumTiles() + tileIndex + 1];
    }
    TileWall getNorthWallAtTile(TileIndex tileIndex) const {
        const i32 floorStride = mTileSpatialGrid->getFloorStride();
        if (((tileIndex % floorStride) / mTileSpatialGrid->getDims().x) >= (mTileSpatialGrid->getDims().y - 1)) {
            return TileWall();
        }
        return mWalls[tileIndex + mTileSpatialGrid->getDims().x];
    }
    // Most optimized way to query/iterate tiles as it avoids bounds checking on +x or +y
    void getSouthAndWestWallsAtTile(TileWall outWalls[2], TileIndex tileIndex) const {
        outWalls[0] = getSouthWallAtTile(tileIndex);
        outWalls[1] = getWestWallAtTile(tileIndex);
    }
    void getWallsAtTile(TileWalls& outWalls, TileIndex tileIndex) const {
        outWalls.south = getSouthWallAtTile(tileIndex);
        outWalls.west = getWestWallAtTile(tileIndex);
        outWalls.east = getEastWallAtTile(tileIndex);
        outWalls.north = getNorthWallAtTile(tileIndex);
    }
    void setWallsAtTile(TileIndex index, TileWall walls[4]) {
        setSouthWallAtTile(index, walls[e_cast(Cartesian::SOUTH)]);
        setWestWallAtTile(index, walls[e_cast(Cartesian::WEST)]);
        setEastWallAtTile(index, walls[e_cast(Cartesian::EAST)]);
        setNorthWallAtTile(index, walls[e_cast(Cartesian::NORTH)]);
    }
    void setSouthWallAtTile(TileIndex tileIndex, TileWall wall) {
        mWalls[tileIndex] = wall;
    }
    void setWestWallAtTile(TileIndex tileIndex, TileWall wall) {
        mWalls[mTileSpatialGrid->getNumTiles() + tileIndex] = wall;
    }
    void setEastWallAtTile(TileIndex tileIndex, TileWall wall) {
        if ((tileIndex % mTileSpatialGrid->getDims().x) >= (mTileSpatialGrid->getDims().x - 1)) {
            return;
        }
        mWalls[mTileSpatialGrid->getNumTiles() + tileIndex + 1] = wall;
    }
    void setNorthWallAtTile(TileIndex tileIndex, TileWall wall) {
        const i32 floorStride = mTileSpatialGrid->getFloorStride();
        if (((tileIndex % floorStride) / mTileSpatialGrid->getDims().x) >= mTileSpatialGrid->getDims().y - 1) {
            return;
        }
        mWalls[tileIndex + mTileSpatialGrid->getDims().x] = wall;
    }

    void setWallAtTile(TileIndex tileIndex, TileWall wall, Cartesian cartesian) {
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
private:
    // Stored horizontal then vertical
    std::vector<TileWall> mWalls;
    const TileSpatialGrid* mTileSpatialGrid = nullptr;
};
