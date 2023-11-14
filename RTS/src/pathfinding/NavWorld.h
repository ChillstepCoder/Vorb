#pragma once

#include "world/ChunkID.h"
#include "CoarseNavGraph.h"

#include "tile/TileHandle.h"
#include "util/ThreadSafeDirtySet.h"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>

#include "tile/TileContainerEvents.h"

#include "terrain/CompressedHeight.h"

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

typedef bg::model::point<i32, 2, bg::cs::cartesian> NavBoxPoint;
typedef bg::model::box<NavBoxPoint> NavBBox;

class World;
class Chunk;
class TileContainer;
struct TileFineNavData;
struct CoarseNavNode;
struct TileWalls;

constexpr int MAX_NAV_NODE_COUNT = UINT8_MAX;
constexpr ui16 INVALID_DJ_NODE_ID = UINT16_MAX;


//const ui16v2 NAV_NODE_EDGE_OFFSETS[4] = {
//    {0, 0}, // DOWN
//    {0, 0}, // LEFT
//    {15, 0}, // RIGHT
//    {0, 15}  // UP
//};
//
//struct VerticalCoarseNavNodeEdge {
//    TileContainer* adjacentContainer;
//    TileIndex adjacentIndex;
//    bool isUp;
//};

// An edge determines where we can move OUT or IN to this nav node
// We can only traverse an EXTERNAL_EDGE if there is an adjacent edge on the other side



struct CoarseTileEdgePointer {
    CoarseTileEdgePointer() : bottom(UINT32_MAX), left(UINT32_MAX), right(UINT32_MAX), up(UINT32_MAX) {}
    union {
        struct {
            ui32 bottom;
            ui32 left;
            ui32 right;
            ui32 up;
        };
        ui32 edges[4];
    };
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
        zPositionOffsetFromFloor = 0.0f;
        accessBits = 0;
        edgeTypeCartesian = 0;
        pathWeight = 255;
        isOwned = false;
    }
    f32 zPositionOffsetFromFloor = 0.0f;
    ui8 accessBits = 0; // from diagonal left to diagonal up right
    ui8 edgeTypeCartesian = 0; // Each cartesian gets 2 bits 0 = flat, 1 = down, 2 = up, 3 = exterior
    ui8 pathWeight = 255;
    bool isOwned = false;

};
static_assert(sizeof(TileFineNavData) == 8, "Keep tiny");

// Stores all external edges for a container
typedef std::vector<std::pair<TileIndex, Cartesian>> StructureExternalEdgeList;
// Stores external edges mapped to chunk locations
typedef std::map<LiteChunkID, StructureExternalEdgeList> StructureExternalEdgeListOutput;
// TODO: Vector of Vector may be better here for memory footprint + iteration?
typedef std::unordered_map<TileContainerID, StructureExternalEdgeList> ContainerTerrainDependentEdges;
struct ContainerNavData {
    ContainerNavData() = default;
    ContainerNavData(
        CoarseNavGraph&& coarseNavGraph, std::vector<TileFineNavData>&& fineNavGraph, i32v3 worldPos, i32v3 containerDims, i32 floorHeight, TileContainerID containerId)
        : coarseNavGraph(std::move(coarseNavGraph)), fineNavGraph(std::move(fineNavGraph)), worldPos(worldPos), containerDims(containerDims), floorHeight(floorHeight), containerId(containerId)
    { }

    VORB_NON_COPYABLE_BUT_MOVABLE(ContainerNavData);

    CoarseNavGraph coarseNavGraph;
    std::vector<TileFineNavData> fineNavGraph;
    i32v3 worldPos;
    i32v3 containerDims;
    i32 floorHeight;
    TileContainerID containerId;

    i32v3 getTileWorldPos(TileIndex index) const {
        assert(index < fineNavGraph.size());
        const i32 floorStride = containerDims.x * containerDims.y;
        return worldPos + i32v3(index % containerDims.x, (index % floorStride) / containerDims.x, (index / floorStride) * floorHeight + fineNavGraph[index].zPositionOffsetFromFloor);
    }

    i32v3 getTileXYZOffsetWithZScale(TileIndex index) const {
        const i32 floorStride = containerDims.x * containerDims.y;
        return i32v3(index % containerDims.x, (index % floorStride) / containerDims.x, (index / floorStride) * floorHeight);
    }
};

struct NavGraphBuildTaskData {
    StructureExternalEdgeListOutput externalEdges;
    std::vector<TileFineNavData> fineNavData;
    NavGraphTileDataToCopy navTileData;
    CoarseNavGraph navGraph;
    const TileContainer* container;
};

struct ContainerNavRegion {
    NavBBox box;
    TileContainerID id;

    bool operator==(const ContainerNavRegion& rhs) const {
        return (id == rhs.id) && (memcmp(&this->box, &rhs.box, sizeof(box)) == 0);
    }
};
// https://stackoverflow.com/questions/64179718/storing-or-accessing-objects-in-boost-r-tree
template <>
struct bgi::indexable<ContainerNavRegion>
{
    typedef NavBBox result_type;
    NavBBox operator()(const ContainerNavRegion& c) const { return c.box; }
};

class TerrainExternalEdges {
public:
    inline bool isExternal(TileIndex tileIndex, Cartesian cartesian) {
        return edgeData.getBit(tileIndex * 4 + e_cast(cartesian));
    }
    void setExternal(TileIndex tileIndex, Cartesian cartesian) {
        edgeData.setBit(tileIndex * 4 + e_cast(cartesian));
    }

private:
    StaticBitArray<CHUNK_SIZE * 4> edgeData; // 8kb
};

// TODO: Lazy navgraph generation?
class NavWorld
{
public:
    NavWorld(World& world);
    ~NavWorld();

    void tickGameThread();
    void updateNavThread();

    void buildNavGraphForContainer(const TileContainer& tileContainer, OPT TerrainExternalEdges* terrainExternalEdges);

    // ========== Debug drawing ==========
    void debugDrawCoarseNavGraphForContainer(const TileContainer& tileContainer, OPT const CompressedHeight* heightData, ui32 lifetime, int debugId = 0) const;
    void debugDrawFineNavGraphForContainer(const TileContainer& tileContainer, ui32 lifetime, int debugId = 0) const;
    void debugDrawCoarseNavNode(const TileHandle& tileHandle, OPT const CompressedHeight* heightData, ui32 lifetime, int debugId = 0) const;

    const CoarseNavGraph* tryGetCoarseNavGraph(TileContainerID containerId) const;
    const CoarseNavGraph& getCoarseNavGraph(TileContainerID containerId) const;

    const CoarseNavNode* getCoarseNavNode(CoarseNavNodeIndexPair index) const;
    const CoarseNavNode* getCoarseNavNode(TileContainerID containerId, ui16 navNodeIndex) const;
    TileFineNavData getFineNavData(TileContainerID containerId, TileIndex tileIndex) const;
    TileFineNavData getFineNavDataAndContainerDims(TileContainerID containerId, TileIndex tileIndex, OUT i32v3& containerDims, OUT i32& floorHeight) const;
    const ContainerNavData& getNavDataForContainer(TileContainerID containerId) const;

    // Spatial lookup
    LiteTileHandle getTileHandleAndNavDataAtWorldPos(const i32v3& worldPos, OUT const ContainerNavData** outNavData) const;

    World& getWorld() const { return mWorld; }

    void markContainerNavDirty(TileContainer* container);
    bool navThreadTryReserveHarvestable(LiteTileHandle position) const;
private:
    void finishNavGraphBuildTask(NavGraphBuildTaskData& taskData);
    void initEventHandlers();
    bool trySetFineNavEdgeCartesian(TileIndex tileIndex, TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, const i32v3& containerDims, const std::vector<Tile>& tiles, const BitArray& ownedTiles, const f32 groundZPosition, const f32 floorHeight, TileFineNavData& tileFineNavData, int prevZ, StructureExternalEdgeList* externalEdges);
    bool trySetFineNavEdgeCartesianDiagonal(TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, const i32v3& containerDims, const std::vector<Tile>& tiles, const BitArray& ownedTiles, const f32 groundZPosition, TileFineNavData& fineNavData);

    void markChunkContainerNavDirty(LiteChunkID chunkId);

    //void buildEdges(TileContainer& tileContainer, const int cornerX, const int cornerY, const int zPos, const i32v2& subchunkDims, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, CoarseNavNodeIndex* navNodeIdTable, std::vector<CoarseNavNode>& navNodes, Cartesian dir);
    //void addNodeEdge(TileContainer& tileContainer, CoarseNavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<CoarseNavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir);

    // Return true if a new edge was made
    bool tryBuildCoarseEdge(NavGraphTileDataToCopy& navTileData, const TileFineNavData& fineNavData, const TileIndex index, const TileIndex prevIndex, TileIndex outerIndex, const i32v3& containerDims, const std::vector<Tile>& tiles, const BitArray& ownedTiles, const ui16 navNodeIndex, std::vector<CoarseTileEdgePointer>& tileEdgePointers, std::vector<std::vector<CoarseNavNodeEdge>>& nodeEdges, const Cartesian dir, bool isBorder, bool canExtendPrevEdge);

    // TODO: We need to destroy these on chunk destruct
    moodycamel::ConcurrentQueue<NavGraphBuildTaskData> mFinishedNavGraphBuildTasks;
    std::unordered_map<TileContainerID, ContainerNavData> mNavGraphs;
    bgi::rtree<ContainerNavRegion, bgi::quadratic<16>> mSpatialLookup;
    mutable boost::container::flat_map<LiteTileHandle, TimeStampSec> mReservedHarvestables;

    enum class ChunkDependencyFlags : ui8 {
        CHUNK_DEPENDENCY_0 = BIT(0),
        CHUNK_DEPENDENCY_1 = BIT(1),
        CHUNK_DEPENDENCY_2 = BIT(2),
        CHUNK_DEPENDENCY_3 = BIT(3)
    };
    struct TileContainerToDestroy {
        i32v3 worldPos;
        i32v2 dims;
        TileContainerID id;
        bool isTerrain;
        BitFlags<ChunkDependencyFlags> chunkDependencyFlags;
    };
    
    GameThreadBatchedDirtySet<const TileContainer*> mDirtyTileContainers;
    GameThreadBatchedDirtyVector<TileContainerToDestroy> mContainersToDestroy;

    TileContainerListeners mTileContainerEventListeners;

    World& mWorld;

    // Large data at the bottom
    std::unique_ptr<TileContainerID[]> mTerrainTileContainers;
    std::unordered_map<LiteChunkID, ContainerTerrainDependentEdges> mTerrainDependentEdges;

};