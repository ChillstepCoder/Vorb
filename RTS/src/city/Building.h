#pragma once

#include "CityConst.h"
#include "city/BuildingGrammar.h"
#include "util/BitArray.h"

#include "city/CityPlot.h"

#include "definitions/BuildingDef.h"
#include "rendering/mesh/Mesh.h"
#include "tile/TileContainer.h"

#include "structure/Structure.h"


class Mesh;
class btTriangleIndexVertexArray;

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

    // TODO: Boost allocator

    const BitArray& getInteriorTilesInAABB() const { return mTileContainer->getOwnedTiles(); }
    const std::vector<RoomNode>& getRooms() const { return mRooms; }
    const std::map<TileIndex, RoomNodeID>& getDoorTiles() const { return mDoorTiles; }

private:
   
    std::vector<RoomNode> mRooms;
    CityPlotIndex mPlotIndex = INVALID_PLOT_INDEX;
    BuildingFunction mFunction = BuildingFunction::NONE;
    BuildingID mId = INVALID_BUILDING_ID;

    std::map<TileIndex, RoomNodeID> mDoorTiles;
    // Entity owning this plot, can be a person or a business
    entt::entity mOwnerEntity = INVALID_ENTITY;

    // TODO: Move to Business?
    //ItemTradeManager mTradeManager; // TODO: This is a large copy and we pass building by value
};

enum class RoadType {
    DIRT,
    PAVED
};

struct CityRoad {
    ui32v2 startPos;
    ui32v2 endPos;
    i32AABB2 aabb;
    ui32 width;
    ui32 length;
    RoadType type = RoadType::PAVED;
    RoadID id;
    AXIS_2D axis;

    std::vector<std::pair<ui32, CityRoad*>> neighborRoads;
    bool mIsBuilt = false;
};

