#pragma once

class IWorld;

// TODO: MOVE TO FISHREPOSITORY
typedef ui32 FishID;

#include <boost/container/flat_map.hpp>

#include "world/IChunkGrid.h"

constexpr int FISH_CELLS_WIDTH = 2;
constexpr int FISH_CELLS_PER_CHUNK = FISH_CELLS_WIDTH * FISH_CELLS_WIDTH;
constexpr int FISH_CELL_TILE_WIDTH = CHUNK_WIDTH / FISH_CELLS_WIDTH;
constexpr int FISH_CELL_HALF_TILE_WIDTH = FISH_CELL_TILE_WIDTH / 2;
constexpr int FISH_CELL_TILE_SIZE = FISH_CELL_TILE_WIDTH * FISH_CELL_TILE_WIDTH;

typedef boost::container::flat_map<FishID, int> FishPopulationMap;

struct FishDef;
class FishRepository;

struct FishPopulation {
    FishID mFishId;
    int mNumFish;
};

struct ActiveFish {
    FishID mFishId = INVALID_FISH_ID;
    f32v3 mVelocity = f32v3(0.0f);
    f32v3 mPosition = f32v3(0.0f);
    f32 mRotation = 0.0f;
};

struct InactiveFish {
    FishID mFishId;
    TileIndex mTileIndex;
};

struct FishCell {
    i32v2 mWorldPos;
    std::vector<ActiveFish> mFish; // LOD rendered
    FishPopulationMap mPopulations;
    BitArray mSpawnableTiles;
    int mTotalSpawnableTiles = 0; // can derive max population and population pressure from this
    bool mRenderingFish = false; // When false, we do not need to update fish
    mutable std::mutex mMutex;
};

struct DormantFishCell {
    FishPopulationMap mPopulations;
};

struct FishChunk {
    FishCell mCells[FISH_CELLS_PER_CHUNK];
};
typedef std::unique_ptr<FishChunk> FishChunkPtr;

struct DormantFishChunk {
    DormantFishCell mCells[FISH_CELLS_PER_CHUNK];
    TimePoint mUnloadedTime; // TODO: GameTime for load/save
};

class FishEcosystem
{
    friend class FishRenderer;
public:
    FishEcosystem(IWorld& world);
    ~FishEcosystem();

    void tickGameThread();

    void initChunkFish(Chunk& chunk);
private:
    void initEventHandlers();
    void disposeChunkFish(Chunk& chunk);
    void makeDormant(FishChunk& chunk, DormantFishChunk& dormantChunk);
    void makeUnDormant(FishChunk& chunk, DormantFishChunk& dormantChunk);
    bool trySpawnFish(FishCell& cell, const FishDef& fishDef);
    void updateActiveFish();

    boost::container::flat_map<ChunkID, FishChunkPtr> mActiveFishChunks;

    std::mutex mDormantMutex;
    boost::container::flat_map<ChunkID, DormantFishChunk> mDormantFishChunks;

    std::mutex mGenerationMutex;
    boost::container::flat_map<ChunkID, FishChunkPtr> mGeneratedFishChunks;

    IWorld& mWorld;
    FishRepository& mFishRepository;

    ChunkGridListeners mChunkGridEventListeners;

    // Very slow ticking for dormancy updates
    TickingTimer mDormancyUpdateTicker = TickingTimer(1000.0f);
};

