#pragma once

#include "tile/Tile.h"
#include "tile/TileHandle.h"
#include "util/BitArray.h"
#include "tile/TileContainerEvents.h"

#include "physics/StaticPhysicsMesh.h"

class btRigidBody;
class Mesh;


enum DynamicTileType : ui8 {
    // Walls (Keep first)
    WALL_SOUTH = e_cast(Cartesian::SOUTH),
    WALL_WEST = e_cast(Cartesian::WEST),
    WALL_EAST = e_cast(Cartesian::EAST),
    WALL_NORTH = e_cast(Cartesian::NORTH),
    WALL_TERM = WALL_NORTH,
    // Layers
    TILE_GROUND,
    TILE_MID,
    TILE_TOP
};
enum DynamicTileFlags : ui8 {
    ACTIVE = 1 << 0,
};

struct DynamicTile {
    TileIndex mTileIndex;
    BitFlags<DynamicTileFlags> mFlags;
    DynamicTileType mType;
};
static_assert(sizeof(DynamicTile) == 8, "Keep small");

struct TileContainerEntranceEdge {
    ui16 adjacentIndex;
    ui16 distanceTiles;
};

struct TileContainerEntrance {
    mutable std::vector<TileContainerEntranceEdge> adjacentEntrances; // TODO: Smaller data structure
    TileIndex tileIndex;
    bool isLocked; // TODO: Access type enum?
};

enum class TileFineNavEdgeType : ui8 {
    NONE = 0,
    DOWN = 1,
    UP = 2,
    EXTERIOR = 3,
};

struct TileFineNavData {

    void setCanAccessDirection(Cartesian8 dir8, bool canAccess) {
        const ui8 bitShift = e_cast(dir8);
        const ui8 bitMask = 1ui8 << bitShift;
        accessBits = (accessBits & (~bitMask)) | (canAccess << bitShift);
    }
    bool canAccessDirection(Cartesian8 dir8) const {
        return accessBits & 1ui8 << e_cast(dir8);
    }
    void setEdgeType(Cartesian dir, TileFineNavEdgeType edgeType) {
        const ui8 bitShift = e_cast(dir) * 2ui8;
        const ui8 bitMask = 0b11 << bitShift;
        edgeTypeCartesian = (edgeTypeCartesian & (~bitMask)) | (e_cast(edgeType) << bitShift);
    }
    TileFineNavEdgeType getEdgeType(Cartesian dir) const {
        const ui8 bitShift = e_cast(dir) * 2ui8;
        const ui8 bitMask = 0b11 << bitShift;
        return TileFineNavEdgeType((edgeTypeCartesian & bitMask) >> bitShift);
    }

    void reset() {
        accessBits = 0;
        edgeTypeCartesian = 0;
        pathWeight = 255;
    }

    ui8 accessBits = 0; // from diagonal left to diagonal up right
    ui8 edgeTypeCartesian = 0; // Each cartesian gets 2 bits 0 = flat, 1 = down, 2 = up, 3 = exterior
    ui8 pathWeight = 255;

};
static_assert(sizeof(TileFineNavData) == 3, "Keep tiny");

EVENT_DISPATCHER_TYPE(TileContainer, TileContainerEventType, const TileContainerEvent&);

enum class TileContainerState : ui8 {
    LOADING,
    WAITING_MESH_AND_PHYSICS,
    READY
};

class TileContainer;
// Static class
class TileContainerRepository {
public:
    static TileContainer* getNewTileContainer(const ui32v3& rootPos, const ui32v3& dims, ui32 floorHeight, bool isTerrain);
    static void destroyTileContainer(TileContainer* container);
    
    static TileContainer* getTileContainer(TileContainerID id);

    static std::vector<std::unique_ptr<TileContainer>>& getTileContainers();

    STATIC_EVENT_LISTENER_FUNCS(TileContainer, Ready, TileContainerEventType::Ready, const TileContainerEvent&);
    STATIC_EVENT_LISTENER_FUNCS(TileContainer, EditTile, TileContainerEventType::EditTile, const TileContainerEvent&);
    STATIC_EVENT_LISTENER_FUNCS(TileContainer, Destroy, TileContainerEventType::Destroy, const TileContainerEvent&);
    STATIC_EVENT_DISPATCHER(TileContainer);
};

// TODO: Memory recycler?
class TileContainer
{
public:
    friend struct TileRef;
    friend struct TileHandle;
    friend class TileContainerRepository;
    friend class ChunkGenerator;
    friend class NavThread; // TODO: Too many friends?
    friend class NavWorld; // TODO: Remove
    friend class IChunkGrid;
    friend class PathFinder;
    TileContainer() = default;
    ~TileContainer();
    VORB_NON_COPYABLE_BUT_MOVABLE(TileContainer);

private:

    void init(TileContainerID id, ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, bool isTerrain);
    void freeData();

public:
    void allocateData();

    void updateActiveDynamicTiles();

    // =========== Tile mutators ===========
    bool canAddTileData(TileIndex i, const TileData& tileData) const;
    void addTileLayer(TileIndex i, const TileData& tileData);
    bool tryAddTileLayer(TileIndex i, const TileData& tileData);
    void setTileLayer(TileIndex i, TileLayer layer, TileID id);
    void setTileFlag(TileIndex i, TileFlags flag);
    void setTileFlags(TileIndex i, TileFlags flags);
    void clearTileFlag(TileIndex i, TileFlags flag);
    void clearTileFlags(TileIndex i);
    void setTileGroundZPosition(TileIndex i, f32 groundZPosition);
    void setTileOrientation(TileIndex i, Cartesian dir, TileLayer layer);
    void setWallAt(TileIndex index, Cartesian dir, TileWall wall);
    void setWallsAt(TileIndex index, TileWalls walls);

    const TileWalls& getWallsMainThread(TileIndex i) const { return mWalls[i]; }
    const std::vector<TileWalls>& getTileWalls() const { return mWalls; }

    const std::vector<DynamicTile>& getDynamicTiles() const { return mDynamicTiles; }

    // This needs to be floor(f32v3worldPos)
    TileHandle tryGetTileHandleAtWorldPos(const i32v3& worldPos) const;

    // =========== Tile accessors  ===========
    Tile& getMutableTileAt(TileIndex i) {
        assert(IS_GAME_THREAD());
        assert(i < mTiles.size());
        return mTiles[i];
    }

    const Tile& getTileAt(TileIndex i) const {
        assert(i < mTiles.size());
        return mTiles[i];
    }
    const Tile& getTileAt(ui32 offsetX, ui32 offsetY, ui32 offsetZ) const {
        const TileIndex i = getTileIndexFromXYZOffset(offsetX, offsetY, offsetZ);
        assert(i < mTiles.size());
        return mTiles[i];
    }

    const Tile& getTileAtNoAssert(TileIndex i) const {
        return mTiles[i];
    }
    i32v3 getTileXYZOffsetWithZScale(TileIndex i) const {
        const ui32 layerSize = mDims.x * mDims.y;
        return i32v3(i % mDims.x, (i % layerSize) / mDims.x, (i / layerSize) * mFloorHeight);
    }
    i32v3 getTileXYZOffset(TileIndex i) const {
       const ui32 layerSize = mDims.x * mDims.y;
       return i32v3(i % mDims.x, (i % layerSize) / mDims.x, i / layerSize);
    }
    i32v2 getTileXYOffset(TileIndex i) const {
        const ui32 layerSize = mDims.x * mDims.y;
        return i32v2(i % mDims.x, (i % layerSize) / mDims.x);
    }
    TileIndex getTileIndexFromXYZOffset(const ui32v3& xyz) const {
        return xyz.x + xyz.y * mDims.x + xyz.z * mDims.x * mDims.y;
    }
    TileIndex getTileIndexFromXYZOffset(const i32v3& xyz) const {
        return (TileIndex)(xyz.x + xyz.y * mDims.x + xyz.z * mDims.x * mDims.y);
    }
    TileIndex getTileIndexFromXYZOffset(i32 x, i32 y, i32 z) const {
        return (TileIndex)(x + y * mDims.x + z * mDims.x * mDims.y);
    }
    TileContainerID getId() const { return mId; }
    bool isTerrain() const { return mIsTerrain; }

    bool isReady() const { return mState == e_cast(TileContainerState::READY); }
    TileContainerState getState() const { return (TileContainerState)mState.load(); }
    void setState(TileContainerState state) { mState = e_cast(state); }

    // =========== Ownership  ===========
    bool isTileOwned(TileIndex index) const { return mOwnedTiles.getNumBits() == 0 || mOwnedTiles.getBit(index); }
    const BitArray& getOwnedTiles() const { return mOwnedTiles; }
    void allocateOwnedTiles() { mOwnedTiles.resizeAndZero(mDims.x * mDims.y * mDims.z); assert(!mOwnedTiles.isEmpty()); }
    void setOwnedTile(TileIndex index) { mOwnedTiles.setBit(index); }
    void clearOwnedTile(TileIndex index) { mOwnedTiles.clearBit(index); }
    void setOwnedTileTo(TileIndex index, bool isOwned) { mOwnedTiles.setBitTo(index, isOwned); }

    // =========== Refcount  ===========
    inline void incRef() const {
        assert(IS_GAME_THREAD()); // Only main thread is allowed to incref
        assert(mRefCount.load() < 2000u); // This is probably a sign of something really awful
        ++mRefCount;
        if (mRefCount > 400) {
            std::cout << "DETECTED " << mRefCount << " REF COUNTS ON TILE CONTAINER " << std::endl;
            assert(false && "Too many container refcounts");
        }
    }
    inline void decRef() const {
        assert(mRefCount.load());
        --mRefCount;
    }
    ui32 getRefCount() const { return mRefCount; }

    bool didInitMeshPhysicsAndNav() const { return mDidInitNav && mDidInitMesh && mDidInitPhysics; }
    void setDidInitMesh() { mDidInitMesh = true; }
    void setDidInitPhysics() { mDidInitPhysics = true; }
    void setDidInitNav() { mDidInitNav = true; }

    // =========== Dirty bits  ===========
    bool isDirtyData() const { return mDirtyData; }
    void setDirtyData() { mDirtyData = true; }
    void clearDirtyData() { mDirtyData = false; }

    // =========== Accessors  ===========
    const i32v2& getWorldPos2D() const { return reinterpret_cast<const i32v2&>(mRootPos); }
    const i32v3& getWorldPos3D() const { return mRootPos; }
    const f32v3 getWorldPosCenter3D() const { return f32v3(mRootPos) + f32v3(mDims) * 0.5f; }
    const i32v3& getDims() const { return mDims; }
    ui32 getFloorHeight() const { return mFloorHeight; }

    const std::vector<Tile>& getTiles() const { return mTiles; }
    const std::vector<TileWalls>& getWalls() const { return mWalls; }
    const std::vector<TileFineNavData>& getFineNavData() const { return mFineNavData; }

    // Nav
    const std::vector<TileContainerEntrance>& getEntrances() const { return mEntrances; }
    void addEntrance(TileIndex pos, bool isLocked);
    void removeEntrance(TileIndex pos);

private:
    void onTileChanged(TileIndex tileIndex);

    void addDoor(Cartesian doorSide, TileIndex tileIndex);
    void removeDoor(Cartesian doorSide, TileIndex tileIndex);

    BitArray mOwnedTiles;
    // TODO: Can we use arrays instead of vectors to shrink these a bit?
    std::vector<Tile> mTiles; // TODO: Memory recycler and or compression
    std::vector<TileWalls> mWalls; // TODO: Memory recycler and or compression
    std::vector<DynamicTile> mDynamicTiles; // TODO: Memory recycler and or compression
    std::vector<ui16> mActiveDynamicTiles; // Iterate and update
    std::vector<TileFineNavData> mFineNavData;
    std::vector<TileContainerEntrance> mEntrances;
    TileContainerID mId;
    i32v3 mDims;
    i32v3 mRootPos;
    ui32 mFloorHeight = 3u;
    mutable std::atomic_uint32_t mRefCount = 0u;
    std::atomic_bool mDidInitMesh = false;
    std::atomic_bool mDidInitPhysics = false;
    std::atomic_bool mDidInitNav = false;

    std::atomic_uint8_t mState = e_cast(TileContainerState::LOADING);
    bool mDirtyData = false;
    bool mIsTerrain = false;
};
