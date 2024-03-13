#pragma once

#include "math/Random.h"

typedef ui16 RoadEdgeID;
constexpr RoadEdgeID INVALID_ROAD_EDGE = std::numeric_limits<RoadEdgeID>::max();

class World;
class RandomGenerator;
class VisualLog;

// TODO: RoadGrid needs access to this
enum class RoadType : ui8 {
    Dirt,
    COUNT
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
    f32v2 direction;
    f32 length;
    RoadSegmentType segmentType = RoadSegmentType::InfiniteLine;
    ui8 widthTiles[2]; // Allow taper
    bool infiniteEdges[2]; // Whether each vertex implicitly extends to infinity
    RoadType roadType;
};

// Minimum span for quick tracing
//struct RoadSegmentTraceInfo {
//    DTileCoord verts[2];
//    bool infiniteEdges[2] = {};
//    RoadSegmentType type;
//};

struct SettlementSector {
    SettlementSector() = default;
    SettlementSector(DTileCoord center, f32 desiredRadius, SettlementSectorID id) : center(center), desiredRadius(desiredRadius), id(id) {}

    std::vector<std::pair<ui32 /*sectionId*/, RoadEdgeID>> neighborSections;
    DTileCoord center;
    f32 desiredRadius;
    SettlementSectorID id;
};

struct RoadSegmentHitResult {
    RoadSegmentID hitSegmentId = INVALID_ROAD_SEGMENT_ID;
    f32 timeSource;
    f32 timeTarget;

    bool isValid() const { return hitSegmentId != INVALID_ROAD_SEGMENT_ID; }
};

struct RoadSegmentIntersectBestHits {
    RoadSegmentHitResult negativeSegmentHit;
    RoadSegmentHitResult negativeInfiniteHit;
    RoadSegmentHitResult positiveSegmentHit;
    RoadSegmentHitResult positiveInfiniteHit;
};

class SettlementRoadNetworkNew {
    friend class SettlementLayoutManager;
private:
    // Return true if we hit ANY segment, ignores infinite edges
    bool simpleTraceAgainstSolidRoadSegments(f32v2 start, f32v2 end);
    bool simpleTraceAgainstSolidRoadSegmentsWithExclusions(DTileCoord start, DTileCoord end, std::span<RoadSegmentID> exclusions);
    // Trace an infinite line to all solid and infinite segments and get closest hit in each direction
    // Returns std::pair<negative, positive> where hitSegmentId == INVALID_ROAD_SEGMENT if no hit
    RoadSegmentIntersectBestHits getBestRoadSegmentHitsForNewPlacement(DTileCoord start, f32v2 dir);
    bool tryAddRoadBetweenSectorPoints(World& world, entt::entity settlement, DTileCoord sector1Pos, DTileCoord sector2Pos, DTileCoord midPoint, RoadType roadType, ui8 width);

    bool tryPlaceRoadInternal(World& world, entt::entity settlement, RoadSegment&& newSegment);
    void updateRoadSegmentType(RoadSegment& segment);

    std::vector<RoadSegment> roadSegments;
    RandomGenerator randomGenerator;
    VisualLog* mCurrentVisLog = nullptr;
};

// TODO: Inside SettlementLayoutComponent
class SettlementLayoutManager {
public:
    bool tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos);
    bool tryAddSector(entt::entity settlement, DTileCoord center, f32 desiredRadius);

    void debugDraw() const;

    std::vector<SettlementSector> mSectors;
    std::vector<ui32> mOpenSectors; // Sectors that have at least one road edge to infinity
    SettlementRoadNetworkNew mRoadNetwork;
    World* mWorld = nullptr;
    DTileCoord mRootPos;
    VisualLog* mCurrentVisLog = nullptr;
};