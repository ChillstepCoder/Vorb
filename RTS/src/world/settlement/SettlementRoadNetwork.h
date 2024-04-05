#pragma once

#include "util/IntersectionHit.h"
#include "world/settlement/SettlementZone.h"

class World;
class RandomGenerator;
class VisualLog;
class SettlementPlotManager;

// TODO: RoadGrid needs access to this
enum class RoadType : ui8 {
    Dirt,
    COUNT
};

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
    std::vector<RoadPointNeedingConstruct> roadPointsNeedingConstruct; // TODO: Move out? This eventually becomes permanently empty
    std::vector<BuildingID> attachedStructures; // TODO Store attach point so its easy to split roads?
    std::vector<std::pair<RoadSegmentID, f32/*time*/>> attachedEdges;
    std::vector<DTileCoord> segmentVerts;
    f32v2 direction;
    f32 length;
    RoadSegmentType segmentType = RoadSegmentType::InfiniteLine;
    ui8 widthTiles[2]; // Allow taper
    bool infiniteEdges[2]; // Whether each vertex implicitly extends to infinity
    RoadType roadType;
    SettlementZone zone;
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

struct ExternalRoadSegmentBlockedTile {
    DTileCoord pos;
    bool wasPlotSeed;
};

// Handles roads and generates plot seeds
class SettlementRoadNetwork {
    friend class SettlementLayoutManager;
public:
    SettlementRoadNetwork(World& world, RandomGenerator& randomGenerator);
    ~SettlementRoadNetwork();
    void init(SettlementPlotManager& plotManager);
    bool tryAddRoadBetweenSectorPoints(entt::entity settlement, DTileCoord sector1Pos, DTileCoord sector2Pos, DTileCoord midPoint, RoadType roadType, ui8 width, SettlementZone zone);
private:
    // Return true if we hit ANY segment, ignores infinite edges
    bool simpleTraceAgainstSolidRoadSegments(f32v2 start, f32v2 end);
    IntersectionHit2D simpleTraceAgainstSolidRoadSegment(f32v2 start, f32v2 end, const RoadSegment& segment);
    // Trace an infinite line to all solid and infinite segments and get closest hit in each direction
    // Returns std::pair<negative, positive> where hitSegmentId == INVALID_ROAD_SEGMENT if no hit
    RoadSegmentIntersectBestHits getBestRoadSegmentHitsForNewPlacement(DTileCoord start, f32v2 dir);

    bool tryPlaceRoadInternal(entt::entity settlement, RoadSegment&& newSegment);
    void updateRoadSegmentType(RoadSegment& segment);
    void refreshExternalRoadsInternal(RoadSegment& newSegment, entt::entity settlement);

    std::vector<RoadSegment> mRoadSegments;
    // Represents road segments that should connect to other cities or extend to more districts
    boost::container::flat_map<RoadSegmentID, std::pair<bool, bool>> mExternalRoadSegments;
    std::map<RoadSegmentID, std::vector<ExternalRoadSegmentBlockedTile>[2]> mExternalRoadSegmentBlockedTiles;
    World& mWorld;
    RandomGenerator& mRandomGenerator;
    VisualLog* mCurrentVisLog = nullptr;
    SettlementPlotManager* mPlotManager = nullptr;
};
