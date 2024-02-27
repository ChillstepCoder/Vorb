#pragma once


typedef ui16 RoadVertexID;
constexpr RoadVertexID INVALID_ROAD_VERTEX = std::numeric_limits<RoadVertexID>::max();
typedef ui16 RoadEdgeID;
constexpr RoadEdgeID INVALID_ROAD_EDGE = std::numeric_limits<RoadEdgeID>::max();

class World;

// Distance is not cached
struct RoadVertex {
    DTileCoord pos;
    std::pair<RoadEdgeID, RoadVertexID> edges[4] 
        = {{INVALID_ROAD_EDGE,INVALID_ROAD_VERTEX},{INVALID_ROAD_EDGE,INVALID_ROAD_VERTEX},{INVALID_ROAD_EDGE,INVALID_ROAD_VERTEX},{INVALID_ROAD_EDGE,INVALID_ROAD_VERTEX}};
};

enum class RoadType : ui8 {
    Dirt,
    Stone,
    COUNT
};

struct RoadEdgeDetails {
    std::vector<StructureID> attachedStructures; // TODO Store attach point so its easy to split roads?
    RoadVertex verts[2];
    ui8 widthTiles[2]; // Allow taper
    RoadType roadType;
    ui16 roadPointCount;
    ui16 roadPointsNeedingConstruct;
};

class SettlementRoadNetwork {
public:
    // Returns false if no roads could be created
    bool tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos);
    bool tryAddNewRoadSegment(World& world, entt::entity settlement, RoadVertexID baseVertexId, DTileCoord targetPos, ui8 baseWidth, ui8 endWidth);


private:
    // Deletion should be rare as it will be a costly operation (not implemented)
    std::vector<RoadVertex> graph; // 0 = root
    std::vector<RoadEdgeDetails> edgeDetails;
};
