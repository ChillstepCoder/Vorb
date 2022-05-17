#pragma once

// TODO: PCH?

#include "CityConst.h"
#include "city/BuildingGrammar.h"
#include "item/ItemTradeManager.h"
#include "util/BitArray.h"

#include "city/CityPlot.h"

#include "definitions/BuildingDef.h"
#include "rendering/mesh/Mesh.h"
#include "tile/TileContainer.h"

class Mesh;

struct BuildingRenderData {
    BuildingRenderData() = default;
    ~BuildingRenderData();

    VORB_NON_COPYABLE_BUT_MOVABLE(BuildingRenderData);

    std::unique_ptr<Mesh> mMesh;
    bool mMeshDirty = true;
};

// TODO: Can we optimize passing this around so theres no copies?
class Building {
public:
    friend class BuildingRenderer;
    friend class BuildingMesher;
    friend class CityBuilder;
    friend class City;

    Building() {};
    ~Building() {};

    VORB_NON_COPYABLE_BUT_MOVABLE(Building);

    // TODO: Boost allocator

private:
    // Building bounds are a series of corner segments
    TileContainer mTileContainer;
    ui32AABB2 mAABB;
    f32 mZPosFloor;
    f32 mZPosRoof;
    std::vector<RoomNode> mGraph;
    BitArray mOwnedTilesInAABB;
    CityPlotIndex mPlotIndex = INVALID_PLOT_INDEX;
    BuildingFunction mFunction = BuildingFunction::NONE;
    BuildingID mId;
    // Entity owning this plot, can be a person or a business
    entt::entity mOwnerEntity = INVALID_ENTITY;
    mutable BuildingRenderData mRenderData;

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
    ui32AABB2 aabb;
    ui32 width;
    ui32 length;
    RoadType type = RoadType::PAVED;
    RoadID id;
    AXIS_2D axis;

    std::vector<std::pair<ui32, CityRoad*>> neighborRoads;
    bool mIsBuilt = false;
};

