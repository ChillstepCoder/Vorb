#pragma once

#include "tile/TileHarvestable.h"

#include "util/SpatialGrid2D.h"
#include "definitions/BiomeDef.h"

// TODO: BiomeConstants
constexpr i32 BIOME_VERTEX_STRIDE = 8;
constexpr int MAX_PRIMARY_RESOURCES_PER_BIOME = 4;

enum class BiomeFlags : ui8 {
    BASE_BIOME = BIT(0),
};

struct BiomeVertex {

    bool isRoot() const {
        return distanceFromRoot == 0;
    }

    // approx when used by simulation, made exact by active chunks
    ui8 biomeResourceAmountsRemaining[MAX_PRIMARY_RESOURCES_PER_BIOME] = {}; // Only 64 tiles per vertex so [0-64]
    BiomeUniqueID biomeUniqueId = BiomeUniqueID::Ocean;
    BitFlags<BiomeFlags> biomeFlags;
    ui16 distanceFromRoot = 0;
    ui32 rootVertexIndex = 0;
    // A biome vertex owns some number of surrounding tiles. This never changes even if the biome changes.
    // STORE ON TILE INSTEAD. TOO MUCH DATA
    //ui64 ownershipBits[4] = {}; // BL, BR, TL, TR - One bit per tile
};

enum class WorldMarkupBodyType {
    Continent,
    Island,
    Lake,
    Ocean
};

enum class WorldMarkupFlags : ui16 {
    // Body flags
    Continent = BIT(0),
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
    ui32 bodyIndex; // Ocean, island, continent, ect.
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

// Host only?
class BiomeGrid
{
    friend class WorldDataGenerator;
public:
    BiomeGrid(ui32 worldWidthTiles);
    ~BiomeGrid();

    VORB_NON_COPYABLE(BiomeGrid);

    ui32 getTotalVertices() const { return mTotalVertices; }
    ui32 getWidthVertices() const { return mSpatialGrid.getGridWidthCells(); }
    // template <bool THREAD_SAFE>
    const BiomeDef* getBiomeDefAtPoint(f32v2 worldPos) const;
    const WorldMarkupData* getMarkupAtPoint(f32v2 worldPos) const;

    // We will manage the lifetime of the texture
    void setBiomeTexture(VGTexture biomeTexture) { mBiomeTexture = biomeTexture; }
    // Safe to call from render thread
    VGTexture getBiomeTexture() const { return mBiomeTexture; }

    BiomeVertex& getVertexForGeneration(ui32 index) {
        return mGrid[index];
    }
    WorldMarkupData& getMarkupForGeneration(ui32 index) {
        return mMarkup[index];
    }

    void addBodyFromGeneration(WorldBodyMarkupData newBody) {
        mBodies.emplace_back(newBody);
    }
private:
    void setVertex(i32v2 vertexPos, BiomeVertex vertex) {
        mGrid[vertexPos.y * getWidthVertices() + vertexPos.x] = vertex;
    }
    std::vector<WorldBodyMarkupData> mBodies;
    std::unique_ptr<BiomeVertex[]> mGrid;
    std::unique_ptr<WorldMarkupData[]> mMarkup;
    ui32 mTotalVertices;
    SpatialGrid2D mSpatialGrid;
    // Used by terrain to look up biome info
    VGTexture mBiomeTexture = 0;

    struct BiomePrimaryResourceInfo {
        TileHarvestable type;
        ui8 avgMaxInstancesPerBiomeVertex; //[0-64]
        std::pair<TileID, f32/*probability*/> possibleTiles[4];
    };

    // Stores which primary resources exist in each biome
    TileHarvestable mBiomePrimaryResourcesLookup[MAX_PRIMARY_RESOURCES_PER_BIOME][e_count(BiomeUniqueID)];
};

