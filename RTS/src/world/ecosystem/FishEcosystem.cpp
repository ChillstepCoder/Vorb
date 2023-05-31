#include "stdafx.h"
#include "FishEcosystem.h"

#include "resources/ResourceManager.h"
#include "resources/FishRepository.h"

#include "rendering/renderstate/RenderStateManager.h"

#include "world/IWorld.h"
#include "math/Random.h"

constexpr f32 MAX_ZPOS_FISH_SPAWN = -1.0f;
constexpr f32 MAX_DORMANCY_DURATION_SEC = 120.0f;
constexpr f32 FISH_COLLIDE_RADIUS = 0.3f; // TODO: Config

FishEcosystem::FishEcosystem(IWorld& world) :
    mWorld(world),
    mFishRepository(Services::ResourceManager::ref().getFishRepository()) {
    initEventHandlers();

    if (mWorld.getNetMode() != WorldNetMode::DedicatedServer) {
        mRenderStateManager = std::make_unique<RenderStateManager<FishRenderState>>();
    }
}

FishEcosystem::~FishEcosystem() {

}

void FishEcosystem::tickGameThread() {
    PROFILE_FUNCTION();
    mDormancyUpdateTicker.startFrame();
    const f32v2 loadCenter = mWorld.getLoadCenter();

    // Copy generated chunks to active list
    {
        std::lock_guard lock(mGenerationMutex);
        for (auto&& it : mGeneratedFishChunks) {
            mActiveFishChunks[it.first] = std::move(it.second);
        }
        mGeneratedFishChunks.clear();
    }

    if (mDormancyUpdateTicker.tryTick()) {
        std::lock_guard lock(mDormantMutex);
        for (auto&& it = mDormantFishChunks.begin(); it != mDormantFishChunks.end();) {
            // TODO: Distance check too
            const f32 timeSinceLastSpawned = std::chrono::duration<f32>(std::chrono::high_resolution_clock::now() - it->second.mUnloadedTime).count();
            if (timeSinceLastSpawned >= MAX_DORMANCY_DURATION_SEC) {
                it = mDormantFishChunks.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    updateActiveFish();
}

void FishEcosystem::initEventHandlers() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkGridEventListeners);
    // We dont use the ready listener as IChunkGrid will directly call initChunkFish

    chunkGrid.addDestroyListener(mChunkGridEventListeners, [this](Chunk& chunk) {
        ASSERT_GAME_THREAD();
        disposeChunkFish(chunk);
    });

}

void FishEcosystem::initChunkFish(Chunk& chunk) {
    PROFILE_FUNCTION();
    FishChunkPtr newFishChunk = std::make_unique<FishChunk>();
    assert(chunk.getTileContainer());

    newFishChunk->mContainerID = chunk.getTileContainer()->getId();

    f32 timeSinceLastSpawned = FLT_MAX;
    // Retrieve dormant data
    {
        std::lock_guard lock(mDormantMutex);
        auto&& it = mDormantFishChunks.find(chunk.getChunkID());
        if (it != mDormantFishChunks.end()) {
            makeUnDormant(*newFishChunk, it->second);
            timeSinceLastSpawned = std::chrono::duration<f32>(std::chrono::high_resolution_clock::now() - it->second.mUnloadedTime).count();
            mDormantFishChunks.erase(it);
        }
    }

    // TODO: This is fairly inefficient, is it worth cacheing these bits on the chunks during generation instead?
    const std::vector<Tile>& tiles = chunk.getTileContainer()->getTiles();
    for (int cy = 0; cy < FISH_CELLS_WIDTH; ++cy) {
        const int yIndexStart = cy * FISH_CELL_ROW_STRIDE_TILES;
        for (int cx = 0; cx < FISH_CELLS_WIDTH; ++cx) {
            FishCell& cell = newFishChunk->mCells[cy * FISH_CELLS_WIDTH + cx];
            cell.mWorldPos = chunk.getWorldPos() + i32v2(cx * FISH_CELL_TILE_WIDTH, cy * FISH_CELL_TILE_WIDTH);
            cell.mSpawnableTiles.resizeAndZero(FISH_CELL_TILE_SIZE);
            cell.mTotalSpawnableTiles = 0;
            cell.mCellXY = ui8v2(cx, cy);
            const int indexStart = yIndexStart + cx * FISH_CELL_TILE_WIDTH;
            for (int y = 0; y < FISH_CELL_TILE_WIDTH; ++y) {
                for (int x = 0; x < FISH_CELL_TILE_WIDTH; ++x) {
                    const TileIndex tileIndex = indexStart + y * CHUNK_WIDTH + x;
                    if (tiles[tileIndex].getGroundZOffset() <= MAX_ZPOS_FISH_SPAWN) {
                        cell.mSpawnableTiles.setBit(y * FISH_CELL_TILE_WIDTH + x);
                        ++cell.mTotalSpawnableTiles;
                    }
                }
            }
        }
    }

    if (timeSinceLastSpawned < MAX_DORMANCY_DURATION_SEC) {
        // TODO SPAWN FISHIES USING DORMANT DATA
    }
    else {
        // Fresh spawning
        const FishDef& fishDef = mFishRepository.getFish("cod");
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 15; ++j) {
                trySpawnFish(*chunk.getTileContainer(), newFishChunk->mCells[i], fishDef);
            }
        }
    }

    {
        std::lock_guard lock(mGenerationMutex);
        mGeneratedFishChunks[chunk.getChunkID()] = std::move(newFishChunk);
    }
}

void FishEcosystem::disposeChunkFish(Chunk& chunk) {
    PROFILE_FUNCTION();

    // If we are in generation list, make sure to remove
    {
        std::lock_guard lock(mGenerationMutex);
        auto&& it = mGeneratedFishChunks.find(chunk.getChunkID());
        if (it != mGeneratedFishChunks.end()) {
            mGeneratedFishChunks.erase(it);
        }
    }

    auto&& it = mActiveFishChunks.find(chunk.getChunkID());
    if (it != mActiveFishChunks.end()) {
        // Add to dormant list
        {
            std::lock_guard lock(mDormantMutex);
            DormantFishChunk& dormantChunk = mDormantFishChunks[chunk.getChunkID()];
            makeDormant(*it->second, dormantChunk);
        }
        // Remove from active list
        mActiveFishChunks.erase(it);
    }
}

void FishEcosystem::makeDormant(FishChunk& chunk, DormantFishChunk& dormantChunk) {
    dormantChunk.mUnloadedTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < FISH_CELLS_PER_CHUNK; ++i) {
        FishCell& activeCell = chunk.mCells[i];
        DormantFishCell& dormantCell = dormantChunk.mCells[i];
        dormantCell.mPopulations = std::move(activeCell.mPopulations);
    }
}

void FishEcosystem::makeUnDormant(FishChunk& chunk, DormantFishChunk& dormantChunk) {
    for (int i = 0; i < FISH_CELLS_PER_CHUNK; ++i) {
        chunk.mCells[i].mPopulations = std::move(dormantChunk.mCells[i].mPopulations);
    }
}

bool FishEcosystem::trySpawnFish(const TileContainer& container, FishCell& cell, const FishDef& fishDef) {
    int randomTile = Random::getCachedRandom() % FISH_CELL_TILE_SIZE;
    if (cell.mSpawnableTiles.getBit(randomTile)) {
        TileIndex tileIndex = cell.mCellXY.y * FISH_CELL_ROW_STRIDE_TILES + cell.mCellXY.x * FISH_CELL_TILE_WIDTH;
        tileIndex += randomTile % FISH_CELL_TILE_WIDTH + (randomTile / FISH_CELL_TILE_WIDTH) * CHUNK_WIDTH;
        ActiveFish newFish;
        newFish.mFishId = fishDef.mId;
        const f32 groundZ = container.getTileAt(tileIndex).getGroundZOffset();
        // Make sure this is actually underwater
        if (groundZ >= -FISH_COLLIDE_RADIUS) return false;
        const f32 randZPos = -FISH_COLLIDE_RADIUS + (groundZ + FISH_COLLIDE_RADIUS) * Random::xorshf96f();
        newFish.mPosition = f32v3(cell.mWorldPos.x + randomTile % FISH_CELL_TILE_WIDTH, cell.mWorldPos.y + randomTile / FISH_CELL_TILE_WIDTH, randZPos);
        cell.mFish.emplace_back(newFish);
    }
    return false;
}

void FishEcosystem::updateActiveFish() {
    PROFILE_FUNCTION();
    if (mRenderStateManager) {
        FishRenderState& renderState = mRenderStateManager->getRenderStateForUpdate();
        renderState.mActiveCells.resize(mActiveFishChunks.size() * 4);
        size_t renderStateCellIndex = 0;
        for (auto&& activeFishChunk : mActiveFishChunks) {
            for (int i = 0; i < 4; ++i) {
                // Update render state
                FishCell& activeCell = activeFishChunk.second->mCells[i];
                FishRenderStateCell& renderStateCell = renderState.mActiveCells[renderStateCellIndex++];
                renderStateCell.mFish.resize(activeCell.mFish.size());
                renderStateCell.mCellCenter = f32v2(activeCell.mWorldPos) + f32v2(FISH_CELL_HALF_TILE_WIDTH);
                for (size_t j = 0; j < activeCell.mFish.size(); ++j) {
                    ActiveFish& fish = activeCell.mFish[j];
                    fish.mPosition.x += (Random::getCachedRandomf() * 2.0f - 1.0f) * 0.1f;
                    fish.mPosition.y += (Random::getCachedRandomf() * 2.0f - 1.0f) * 0.1f;
                    renderStateCell.mFish[j] = fish;
                }
                
            }
        }
        mRenderStateManager->finishUpdating();
    }
    else {
        // Dedicated server implementation
        assert(false);
    }
}
