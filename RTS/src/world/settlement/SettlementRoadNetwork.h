#pragma once

#include "math/Random.h"

class World;
class RandomGenerator;
class VisualLog;

// TODO: RoadGrid needs access to this
enum class RoadType : ui8 {
    Dirt,
    COUNT
};

typedef ui32 RoadSegmentID;
constexpr RoadSegmentID INVALID_ROAD_SEGMENT_ID = std::numeric_limits<RoadSegmentID>::max();

enum class RoadSegmentType : ui8 {
    Segment,
    PositiveRay,
    NegativeRay,
    InfiniteLine,
    COUNT
};

struct RoadPointNeedingConstruct {
    DTileCoord pos;
    f32 strength;
};

struct RoadSegment {
    std::vector<RoadPointNeedingConstruct> roadPointsNeedingConstruct;
    std::vector<StructureID> attachedStructures; // TODO Store attach point so its easy to split roads?
    std::vector<std::pair<RoadSegmentID, f32/*time*/>> attachedEdges;
    std::vector<DTileCoord> segmentVerts;
    f32v2 direction;
    f32 length;
    RoadSegmentType segmentType = RoadSegmentType::InfiniteLine;
    ui8 widthTiles[2]; // Allow taper
    bool infiniteEdges[2]; // Whether each vertex implicitly extends to infinity
    RoadType roadType;
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


class SettlementRoadNetwork {
    friend class SettlementLayoutManager;
private:
    // Return true if we hit ANY segment, ignores infinite edges
    bool simpleTraceAgainstSolidRoadSegments(f32v2 start, f32v2 end);
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
