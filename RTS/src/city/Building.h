#pragma once

#include "CityConst.h"
#include "city/BuildingGrammar.h"
#include "util/BitArray.h"

#include "city/CityPlot.h"

#include "definitions/BuildingDef.h"
#include "tile/TileContainer.h"

#include "structure/Structure.h"

// TODO: Can we optimize passing this around so theres no copies?
class Building : public Structure {
public:
    friend class BuildingRenderer;
    friend class BuildingMesher;
    friend class CityBuilder;
    friend class City;

    Building();
    ~Building();

    VORB_NON_COPYABLE_BUT_MOVABLE(Building);

    //const std::vector<RoomGenNode>& getRooms() const { return mRooms; }

private:
   
    //std::vector<RoomGenNode> mRooms;
    BuildingFunction mFunction = BuildingFunction::NONE;
    BuildingID mId = INVALID_BUILDING_ID;

    // Entity owning this plot, can be a person or a business
    entt::entity mOwnerEntity = INVALID_ENTITY;

    // TODO: Move to Business?
    //ItemTradeManager mTradeManager; // TODO: This is a large copy and we pass building by value
};

enum class OLDRoadType : ui8 {
    DIRT = 0,
    PAVED = 1
};

struct CityRoad {
    ui32v2 startPos;
    ui32v2 endPos;
    i32AABB2 aabb;
    ui32 width;
    ui32 length;
    OLDRoadType type = OLDRoadType::PAVED;
    RoadID id;
    AXIS_2D axis;

    std::vector<std::pair<ui32, CityRoad*>> neighborRoads;
    bool mIsBuilt = false;
};

