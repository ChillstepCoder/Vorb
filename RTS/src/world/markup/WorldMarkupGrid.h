#pragma once

#include "util/SpatialGrid2D.h"

constexpr i32 MARKUP_VERTEX_STRIDE = 8;

enum class WorldMarkupBodyType {
    LargeIsland,
    Island,
    Lake,
    Ocean,
    COUNT
};

enum class WorldMarkupFlags : ui16 {
    // Body flags
    LargeIsland = BIT(0),
    Island = BIT(1),
    Lake = BIT(2),
    Ocean = BIT(3),

    // Misc
    Settlement = BIT(4),
    PlayerOwned = BIT(5),
    OffLimits = BIT(6), // Settlement owner doesn't want others to come here.
    StructurePlot = BIT(7), // A structure's plot extends here TODO: PLOTS ARE 8x8 (Basically V Rising plot but with free build)
    Road = BIT(8),
    TERM
};

// City generation steps:
// 1. (A) Large plot is made called Hamlet of Commons, shanty houses made from cheap materials. People basically get to erect tents all over it
//    In times of war, a wall is the first thing built by AI before upgrading.
//    This plot evolves and serves as the leaders refuge. Later becomes governors mansion or castle keep.
// 2. (h)(S) New plots are made nearby and houses are built on them. In another direction at a 90 degree angle, permanent stockpiles are built as needed. These are the first permanent structures.
// 3. (A) Hamlet becomes town hall, still allows tents but less of them fit.
// 4. (C) Once many people have houses, a market area is established opposite the town hall.
//    This becomes the economic center, attracting traders and craftsmen. Small businesses and workshops spring up around this area.
//    The market area not only facilitates trade but also serves as a social gathering place, increasing the city's attractiveness to new residents and visitors.
// 5. (I) Industry buildings are made in a new large industry district. This includes workshops, warehouses, and factories, but commerce
//    buildings such as Inns or ops exist here. More houses are added far from industry.

// At any point, Military barracks can be made (X)

// A plot is an 8x8 tile area. Plots chain together into larger plots.
// An nxn area of plots is a District. (To be determined)
// A 4x4 area of districts is a Quarter (5x5?)
// Roads exist on each district, shaped or random intersection point locations. Kinda like tarn adams talk on river entries and exits.
// hIIX
// hhhS
// hChA
// hhhh

// River is often adjacent to the city center

struct WorldMarkupData {
    ui32 bodyIndex = UINT32_MAX; // Ocean, island, continent, ect.
    f32 settleDesirability; // For settlement
    entt::entity owner;
    ui32 continentSize;
    WorldMarkupFlags flags;
    ui16 PADDING;
};

static_assert(sizeof(WorldMarkupData) <= 20, "Keep small");

struct WorldBodyMarkupData {
    WorldMarkupBodyType bodyType;
    ui32 sizeCells; // Cell is 8x8 tiles
};

class WorldMarkupGrid
{
public:
    WorldMarkupGrid(ui32 worldWidthTiles);
    ~WorldMarkupGrid();

    VORB_NON_COPYABLE(WorldMarkupGrid);

    ui32 getTotalVertices() const { return mTotalVertices; }
    ui32 getWidthVertices() const { return mSpatialGrid.getGridWidthCells(); }

    const WorldMarkupData* getMarkupAtPoint(f32v2 worldPos) const;
    const WorldBodyMarkupData* getBodyDataAtPoint(f32v2 worldPos) const;

    WorldMarkupData& getMarkupForGeneration(ui32 index) {
        return mMarkup[index];
    }

    void addBodyFromGeneration(WorldBodyMarkupData newBody) {
        mBodies.emplace_back(newBody);
    }
    void setMarkupReady() {
        mMarkupReady = true;
    }
    bool isMarkupReady() const {
        return mMarkupReady;
    }

private:

    std::vector<WorldBodyMarkupData> mBodies;
    std::unique_ptr<WorldMarkupData[]> mMarkup;
    ui32 mTotalVertices;
    SpatialGrid2D mSpatialGrid;
    std::atomic_bool mMarkupReady = false;
};

