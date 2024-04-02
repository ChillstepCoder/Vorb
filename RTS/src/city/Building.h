#pragma once

#include "CityConst.h"
#include "city/BuildingGrammar.h"
#include "util/BitArray.h"

#include "city/CityPlot.h"

#include "definitions/BuildingDef.h"
#include "tile/TileContainer.h"

#include "structure/Structure.h"

class BuildingBlueprint;

// TODO: Can we optimize passing this around so theres no copies?
class Building : public Structure {
public:
    friend class BuildingRenderer;
    friend class BuildingMesher;
    friend class CityBuilder;
    friend class City;

    Building();
    ~Building();

    Building(Building&& other) noexcept;
    Building& operator=(Building&& other) noexcept;

    VORB_NON_COPYABLE(Building);

    //const std::vector<RoomGenNode>& getRooms() const { return mRooms; }
    void setBlueprint(std::unique_ptr<BuildingBlueprint>&& bp);
    BuildingBlueprint* getBlueprint() const { return mBlueprint.get(); }

private:
   
    //std::vector<RoomGenNode> mRooms;
    //BuildingFunction mFunction = BuildingFunction::NONE;
    BuildingID mId = INVALID_BUILDING_ID;
    std::unique_ptr<BuildingBlueprint> mBlueprint; // If valid, building has not been serialized to disk

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

