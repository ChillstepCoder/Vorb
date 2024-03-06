#pragma once

#include "math/Random.h"

typedef ui16 RoadVertexID;
constexpr RoadVertexID INVALID_ROAD_VERTEX = std::numeric_limits<RoadVertexID>::max();
typedef ui16 RoadEdgeID;
constexpr RoadEdgeID INVALID_ROAD_EDGE = std::numeric_limits<RoadEdgeID>::max();

class World;
class RandomGenerator;

// Distance is not cached
struct RoadVertex {
    DTileCoord pos;
    std::pair<RoadEdgeID, RoadVertexID> edges[4] 
        = {{INVALID_ROAD_EDGE,INVALID_ROAD_VERTEX},{INVALID_ROAD_EDGE,INVALID_ROAD_VERTEX},{INVALID_ROAD_EDGE,INVALID_ROAD_VERTEX},{INVALID_ROAD_EDGE,INVALID_ROAD_VERTEX}};
};

// TODO: RoadGrid needs access to this
enum class RoadType : ui8 {
    Dirt,
    COUNT
};

struct RoadEdgeDetails {
    std::vector<DTileCoord> roadVertsNeedingConstruct;
    std::vector<StructureID> attachedStructures; // TODO Store attach point so its easy to split roads?
    RoadVertexID verts[2];
    ui8 widthTiles[2]; // Allow taper
    RoadType roadType;
    ui16 roadVertCount;
    f32v2 normalDir; // Direction from verts[0] to verts[1]
};

typedef ui32 SettlementSectorID;
constexpr SettlementSectorID INVALID_SECTOR_ID = std::numeric_limits<SettlementSectorID>::max();

typedef ui32 RoadSegmentID;
constexpr RoadSegmentID INVALID_ROAD_SEGMENT_ID = std::numeric_limits<RoadSegmentID>::max();

enum class RoadSegmentType : ui8 {
    Segment,
    PositiveRay,
    NegativeRay,
    InfiniteLine,
    COUNT
};

struct RoadSegment {
    std::vector<DTileCoord> roadPointsNeedingConstruct;
    std::vector<StructureID> attachedStructures; // TODO Store attach point so its easy to split roads?
    std::vector<std::pair<RoadEdgeID, f32/*time*/>> attachedEdges;
    std::vector<DTileCoord> segmentVerts;
    RoadSegmentType segmentType = RoadSegmentType::InfiniteLine;
    ui8 widthTiles[2]; // Allow taper
    bool infiniteEdges[2] = {}; // Whether each vertex implicitly extends to infinity
    RoadType roadType;
    ui16 roadVertCount;
    f32v2 normalDir; // Direction from verts[0] to verts[1]
};

// Minimum span for quick tracing
//struct RoadSegmentTraceInfo {
//    DTileCoord verts[2];
//    bool infiniteEdges[2] = {};
//    RoadSegmentType type;
//};

struct SettlementSector {
    SettlementSector() = default;
    SettlementSector(DTileCoord center, f32 desiredRadius) : center(center), desiredRadius(desiredRadius) {}

    std::vector<std::pair<ui32 /*sectionId*/, RoadEdgeID>> neighborSections;
    DTileCoord center;
    f32 desiredRadius;
    SettlementSectorID id;
};

// TODO: Rename? This encompasses sectors and districts
// A: No, instead wrap this in a SettlementLayoutManager class?
class SettlementRoadNetwork {
public:
    // Returns false if no roads could be created
    bool tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos);
    bool tryAddNewRoadSegment(World& world, entt::entity settlement, RoadType roadType, RoadVertexID baseVertexId, DTileCoord targetPos, ui8 baseWidth, ui8 endWidth);

    bool tryExtrudeRandomRoadSegment(World& world, entt::entity settlement, RoadType roadType, ui8 width, f32 length);
private:
    // Deletion should be rare as it will be a costly operation (not implemented)
    std::vector<RoadVertex> graph; // 0 = root
    std::vector<RoadEdgeDetails> edgeDetails;
    std::vector<RoadVertexID> leafVerts;
    RandomGenerator randomGenerator;
};

struct RoadSegmentHitResult {
    RoadSegmentID segmentId;
    f32 traceTime;
    f32 hitSegmentTime;
};

class SettlementRoadNetworkNew {
    friend class SettlementLayoutManager;
public:
    std::optional<RoadSegmentHitResult> traceAgainstRoadSegments(DTileCoord start, DTileCoord end);
    std::optional<RoadSegmentHitResult> traceAgainstRoadSegmentsAndInfiniteEdges(DTileCoord start, DTileCoord end);

private:
    std::vector<RoadSegmentTraceInfo> 
    std::vector<RoadSegment> roadSegments;
    std::vector<RoadVertexID> leafVerts;
    RandomGenerator randomGenerator;
};

// TODO: Inside SettlementLayoutComponent
class SettlementLayoutManager {
public:
    bool tryAddSector(DTileCoord center, f32 desiredRadius);

    std::vector<SettlementSector> mSectors;
    std::vector<ui32> mOpenSectors; // Sectors that have at least one road edge to infinity
    SettlementRoadNetworkNew mRoadNetwork;
};