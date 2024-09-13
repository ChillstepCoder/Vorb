#pragma once

#include "tile/TileSpatialGrid.h"

#include "serialization/BitseryExt.h"

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

// Holds walls for a TileContainer. The +x and +y outermost edges cannot have walls
// TODO: This could be replaced with an RLE based container for much smaller memory footprint?
class TileWallContainer {
public:
    void init(const TileSpatialGrid* tileSpatialGrid);
    void resizeForCopy(size_t numTiles);
    void copyFrom(const TileWallContainer& other);

    bool isEmpty() const {
        return mWalls.empty();
    }
    // TODO: Serialize
    void destroy();
    TileWall getWallAtTile(TileIndex tileIndex, Cartesian cartesian) const;
    TileWall getSouthWallAtTile(TileIndex tileIndex) const;
    TileWall getWestWallAtTile(TileIndex tileIndex) const;
    TileWall getEastWallAtTile(TileIndex tileIndex) const;
    TileWall getNorthWallAtTile(TileIndex tileIndex) const;
    // Most optimized way to query/iterate tiles as it avoids bounds checking on +x or +y
    void getSouthAndWestWallsAtTile(TileWall outWalls[2], TileIndex tileIndex) const;
    void getWallsAtTile(TileWalls& outWalls, TileIndex tileIndex) const;
    void setWallsAtTile(TileIndex index, TileWall walls[4]);
    void setSouthWallAtTile(TileIndex tileIndex, TileWall wall);
    void setWestWallAtTile(TileIndex tileIndex, TileWall wall);
    void setEastWallAtTile(TileIndex tileIndex, TileWall wall);
    void setNorthWallAtTile(TileIndex tileIndex, TileWall wall);

    void setWallAtTile(TileIndex tileIndex, TileWall wall, Cartesian cartesian);
private:
    // Only allocate memory for walls if they exist
    void allocate();

    // Stored horizontal then vertical
    std::vector<TileWall> mWalls;
    const TileSpatialGrid* mTileSpatialGrid = nullptr;

    BINARY_SERIALIZE() {
        s.ext(mWalls, bitsery::ext::PodStructVector{});
    }
};
