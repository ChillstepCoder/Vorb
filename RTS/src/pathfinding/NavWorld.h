#pragma once

#include "world/ChunkID.h"
#include "CoarseNavGraph.h"

#include "tile/TileHandle.h"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

typedef bg::model::point<i32, 2, bg::cs::cartesian> NavBoxPoint;
typedef bg::model::box<NavBoxPoint> NavBBox;

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

struct ContainerNavData {
    CoarseNavGraph coarseNavGraph;
    std::vector<TileFineNavData> fineNavGraph;
    i32v3 worldPos;
    i32v3 containerDims;
    i32 floorHeight;
    TileContainerID containerId;

    i32v3 getTileWorldPos(TileIndex index) const {
        assert(index < fineNavGraph.size());
        const i32 floorStride = containerDims.x * containerDims.y;
        return worldPos + i32v3(index % containerDims.x, (index % floorStride) / containerDims.x, (index / floorStride) * floorHeight);
    }

    i32v3 getTileXYZOffsetWithZScale(TileIndex index) const {
        const i32 floorStride = containerDims.x * containerDims.y;
        return i32v3(index % containerDims.x, (index % floorStride) / containerDims.x, (index / floorStride) * floorHeight);
    }
};

struct NavGraphBuildTaskData {
    std::vector<TileFineNavData> fineNavData;
    NavGraphTileDataToCopy navTileData;
    CoarseNavGraph navGraph;
    TileContainer* container;
};

struct ContainerNavRegion {
    NavBBox box;
    TileContainerID id;
};

// https://stackoverflow.com/questions/64179718/storing-or-accessing-objects-in-boost-r-tree
template <>
struct bgi::indexable<ContainerNavRegion>
{
    typedef NavBBox result_type;
    NavBBox operator()(const ContainerNavRegion& c) const { return c.box; }
};

// TODO: Lazy navgraph generation?
class NavWorld
{
public:
    NavWorld();

    void updateNavThread();

    void buildNavGraphForContainer(TileContainer& tileContainer);

    // ========== Debug drawing ==========
    void debugDrawCoarseNavGraphForContainer(const TileContainer& tileContainer, OPT const f32* heightData, ui32 lifetime, int debugId = 0) const;
    void debugDrawFineNavGraphForContainer(const TileContainer& tileContainer, OPT const f32* heightData, ui32 lifetime, int debugId = 0) const;
    void debugDrawCoarseNavNode(const TileHandle& tileHandle, OPT const f32* heightData, ui32 lifetime, int debugId = 0) const;

    const CoarseNavGraph* tryGetCoarseNavGraph(TileContainerID containerId) const;
    const CoarseNavGraph& getCoarseNavGraph(TileContainerID containerId) const;

    const CoarseNavNode* getCoarseNavNode(CoarseNavNodeIndexPair index) const;
    const CoarseNavNode* getCoarseNavNode(TileContainerID containerId, ui16 navNodeIndex) const;
    TileFineNavData getFineNavData(TileContainerID containerId, TileIndex tileIndex) const;
    TileFineNavData getFineNavDataAndContainerDims(TileContainerID containerId, TileIndex tileIndex, OUT i32v3& containerDims, OUT i32& floorHeight) const;
    const ContainerNavData& getNavDataForContainer(TileContainerID containerId) const;

    // Spatial lookup
    LiteTileHandle getTileHandleAndNavDataAtWorldPos(const i32v3& worldPos, OUT const ContainerNavData* outNavData) const;

private:
    void setFineNavEdgeCartesian(TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, const i32v3& containerDims, const std::vector<Tile>& tiles, const std::vector<TileWalls>& tileWallsContainer, const BitArray& ownedTiles, const f32 groundZPosition, const f32 floorHeight, TileFineNavData& tileFineNavData, int prevZ);
    void setFineNavEdgeCartesianDiagonal(const TileIndex adjacentIndex, Cartesian8 cartesian8, bool isInner, const i32v3& containerDims, const std::vector<Tile>& tiles, const std::vector<TileWalls>& tileWallsContainer, const BitArray& ownedTiles, const f32 groundZPosition, TileFineNavData& fineNavData);


    //void buildEdges(TileContainer& tileContainer, const int cornerX, const int cornerY, const int zPos, const i32v2& subchunkDims, TileIndex cornerIndex, DisjointSetNode* djNodes, ui32* djNodeIDs, CoarseNavNodeIndex* navNodeIdTable, std::vector<CoarseNavNode>& navNodes, Cartesian dir);
    //void addNodeEdge(TileContainer& tileContainer, CoarseNavNodeIndex* navNodeIdTable, const ui32 djIndex, std::vector<CoarseNavNode>& navNodes, TileIndex corner, TileIndex start, int length, Cartesian dir);

    // Return true if a new edge was made
    bool tryBuildCoarseEdge(NavGraphTileDataToCopy& navTileData, const TileFineNavData& fineNavData, const TileIndex index, const TileIndex prevIndex, TileIndex outerIndex, const i32v3& containerDims, const std::vector<Tile>& tiles, const std::vector<TileWalls>& tileWallsContainer, const BitArray& ownedTiles, const ui16 navNodeIndex, std::vector<CoarseTileEdgePointer>& tileEdgePointers, std::vector<std::vector<CoarseNavNodeEdge>>& nodeEdges, const Cartesian dir, bool isBorder, bool canExtendPrevEdge);

    // TODO: We need to destroy these on chunk destruct
    moodycamel::ConcurrentQueue<NavGraphBuildTaskData> mFinishedNavGraphBuildTasks;
    std::unordered_map<TileContainerID, ContainerNavData> mNavGraphs;
    bgi::rtree<ContainerNavRegion, bgi::quadratic<16>> mSpatialLookup;

    TileContainerID mTerrainTileContainers[WorldData::WORLD_SIZE_CHUNKS];
};