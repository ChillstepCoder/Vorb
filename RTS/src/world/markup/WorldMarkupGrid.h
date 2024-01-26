#pragma once

#include "util/SpatialGrid2D.h"

#include "boost/container/flat_map.hpp"

#include "math/Random.h"

class WorldNameContext;

constexpr i32 MARKUP_VERTEX_STRIDE = 8;

enum class WorldMarkupBodyType : ui8 {
    LargeIsland,
    Island,
    BODY_TYPE_LAND_TERM = Island,
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
    River = BIT(9), // TODO: USE
    TERM
};

constexpr ui16 WORLD_MARKUP_FLAGS_LAND_MASK = e_cast(WorldMarkupFlags::LargeIsland) | e_cast(WorldMarkupFlags::Island);
constexpr ui16 WORLD_MARKUP_FLAGS_WATER_MASK = e_cast(WorldMarkupFlags::Lake) | e_cast(WorldMarkupFlags::Ocean);

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
    ui32 bodyIndex = UINT32_MAX;
    ui32 continentSize;
    BitFlags<WorldMarkupFlags> flags;
    //ui16 PADDING;
};
static_assert(sizeof(WorldMarkupData) == 12, "Keep small");

struct WorldChunkMarkupData {
    ui32 mainLandBodyIndex = UINT32_MAX;
    ui32 mainWaterBodyIndex = UINT32_MAX;
    f32 settleDesirability = 0.0f;
    f32 landRatio = 0.0f; // 1.0 = full land, 0.0 = full water
};

struct WorldBodyNeighborInfo {
    ui32 neighborBodyIndex = UINT32_MAX;
    ui32 adjacentBlocks = 0;
};

struct WorldBodyMarkupData {
    const char* name = nullptr;
    std::vector<i16v2> borderBlocks;
    std::vector<WorldBodyNeighborInfo> neighborBodies;
    std::vector<ChunkID> chunks; // Only used by land bodies, water will be empty
    f32v2 averagePos = f32v2(0.0f);
    ui32 bodyIndex = 0;
    WorldMarkupBodyType bodyType;
    ui32 sizeBlocks = 0; // Block is 8x8 tiles
    bool onMapEdge = false;

    bool isLand() const {
        return bodyType <= WorldMarkupBodyType::BODY_TYPE_LAND_TERM;
    }
};

// Markup is const data created at generation time, it cannot change
class WorldMarkupGrid
{
    friend class MarkupGenerationStage;
public:
    WorldMarkupGrid(ui32 worldWidthTiles, ui32 seed);
    ~WorldMarkupGrid();

    VORB_NON_COPYABLE(WorldMarkupGrid);

    ui32 getTotalVertices() const { return mTotalVertices; }
    ui32 getWidthVertices() const { return mSpatialGrid.getGridWidthCells(); }

    const WorldMarkupData* getMarkupAtPoint(f32v2 worldPos) const;
    const WorldChunkMarkupData* getChunkMarkupAtPoint(f32v2 worldPos) const;
    const WorldBodyMarkupData* getBodyDataAtPoint(f32v2 worldPos) const;

    WorldMarkupData& getMarkupForGeneration(ui32 index) {
        return mMarkup[index];
    } 
    WorldChunkMarkupData& getChunkMarkupForGeneration(ChunkID index) {
        return mChunkMarkup[index];
    }

    const WorldBodyMarkupData& getBodyData(ui32 bodyId) const {
        return mBodies[bodyId];
    }
    WorldBodyMarkupData& getBodyDataForGeneration(ui32 bodyId) {
        return mBodies[bodyId];
    }
    ui32 getNumBodies() const {
        return mBodies.size();
    }

    void addBodyFromGeneration(WorldBodyMarkupData newBody) {
        newBody.bodyIndex = mBodies.size();
        mBodies.emplace_back(newBody);
    }
    void setMarkupReady() {
        mMarkupReady = true;
    }
    bool isMarkupReady() const {
        return mMarkupReady;
    }

    const boost::container::flat_multimap<ui32, ui32 /*body index*/>& getSortedBodies() const {
        return mLandBodiesSortedBySize;
    }

private:
    // Sort bodies and stuff
    void onGenerationComplete();

    boost::container::flat_multimap<ui32 /*size*/, ui32 /*body index*/> mLandBodiesSortedBySize;
    std::vector<WorldBodyMarkupData> mBodies;
    std::unique_ptr<WorldMarkupData[]> mMarkup;
    std::unique_ptr<WorldChunkMarkupData[]> mChunkMarkup;
    std::unique_ptr<WorldNameContext> mNameContext; // Elsewhere?
    ui32 mTotalVertices;
    ui32 mWidthChunks = 0;
    SpatialGrid2D mSpatialGrid;
    std::atomic_bool mMarkupReady = false;
    RandomGenerator gen;
};

