#include "stdafx.h"
#include "FishEcosystem.h"

#include "resources/ResourceManager.h"
#include "resources/FishRepository.h"

#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/PositionComponent.h"
#include "ecs/component/VelocityComponent.h"
#include "ecs/component/YawPitchComponent.h"

#include "rendering/renderstate/RenderStateManager.h"
#include "debugging/DebugRenderer.h"

#include "options/DebugOptions.h"

#include "world/IWorld.h"
#include "math/Random.h"

constexpr f32 MAX_ZPOS_FISH_SPAWN = -1.0f;
constexpr f32 MAX_DORMANCY_DURATION_SEC = 120.0f;
constexpr f32 FISH_COLLIDE_RADIUS = 0.3f; // TODO: Config
constexpr int MAX_PECK_COUNT = 4;
constexpr f32 MIN_PECK_COOLDOWN_TIME = 0.5f;
constexpr f32 MAX_PECK_COOLDOWN_TIME = 2.0f;

FishEcosystem::FishEcosystem(IWorld& world) :
    mWorld(world),
    mFishRepository(Services::ResourceManager::ref().getFishRepository()) {
    initEventHandlers();

    if (mWorld.getNetMode() != WorldNetMode::DedicatedServer) {
        mRenderStateManager = std::make_unique<RenderStateManager<FishChunkRenderStateMap>>();
    }
}

FishEcosystem::~FishEcosystem() {

}

void FishEcosystem::tickGameThread(f32 elapsedSec) {
    PROFILE_FUNCTION();
    mElapsedSec = elapsedSec;

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

entt::entity FishEcosystem::getClosestIdleFishToPoint(f32v3 point, f32 maxRange) const {
    const Chunk* closestChunks[4];
    mWorld.getChunkGrid().getClosestChunksAtPosition(f32v2(point), closestChunks);
    entt::registry& registry = mWorld.getECS().mRegistry;

    f32 closestDistanceSq = SQ(maxRange);
    // TODO: Better search than linear
    entt::entity closestFish = INVALID_ENTITY;
    for (int i = 0; i < 4; ++i) {
        assert(closestChunks[i]);
        const Chunk& chunk = *closestChunks[i];
        if (!chunk.isDataReady()) {
            continue;
        }
        auto&& it = mActiveFishChunks.find(chunk.getChunkID());
        if (it == mActiveFishChunks.end()) {
            continue;
        }
        FishChunk* chunkPtr = it->second.get();
        if (chunkPtr) {
            for (int j = 0; j < chunkPtr->mFish.size(); ++j) {
                entt::entity fishEntity = chunkPtr->mFish[j];
                const PositionComponent& position = registry.get<PositionComponent>(fishEntity);
                const f32 distanceSq = glm::distance2(position.mPosition, point);
                if (distanceSq < closestDistanceSq) {
                    closestDistanceSq = distanceSq;
                    closestFish = fishEntity;
                }
            }
        }
    }
    return closestFish;
}

void FishEcosystem::removeFish(entt::entity fishEntity) {
    assert(fishEntity != INVALID_ENTITY);

    entt::registry& registry = mWorld.getECS().mRegistry;
    FishComponent& fishCmp = registry.get<FishComponent>(fishEntity);
    auto&& it = mActiveFishChunks.find(fishCmp.mResidingChunk);
    if (it != mActiveFishChunks.end()) {
        FishChunk& fishChunk = *it->second;
        for (size_t i = 0; i < fishChunk.mFish.size(); ++i) {
            if (fishChunk.mFish[i] == fishEntity) {
                fishChunk.mFish[i] = fishChunk.mFish.back();
                registry.destroy(fishEntity);
                return;
            }
        }
    }
    assert(false);
}

void FishEcosystem::setFishFollowBobber(entt::entity fishEntity, entt::entity followTarget) {
    assert(fishEntity != INVALID_ENTITY);
    assert(followTarget != INVALID_ENTITY);

    entt::registry& registry = mWorld.getECS().mRegistry;
    FishAIComponent& ai = registry.get<FishAIComponent>(fishEntity);
    ai.mAIState = FishAIState::PeckBobber;
    ai.mFollowTarget = followTarget;
    ai.mPeckCountRemaining = Random::xorshf96() % (MAX_PECK_COUNT + 1);
}

void FishEcosystem::clearFishFollowTarget(entt::entity fishEntity) {
    entt::registry& registry = mWorld.getECS().mRegistry;
    FishAIComponent& ai = registry.get<FishAIComponent>(fishEntity);
    ai.mAIState = FishAIState::Idle;
    ai.mFollowTarget = INVALID_ENTITY;
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
    newFishChunk->mWorldPos = chunk.getWorldPos();
    newFishChunk->mChunkID = chunk.getChunkID();

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
    newFishChunk->mSpawnableTiles.resizeAndZero(CHUNK_SIZE);
    newFishChunk->mTotalSpawnableTiles = 0;
    for (TileIndex tileIndex = 0; tileIndex < CHUNK_SIZE; ++tileIndex) {
        if (tiles[tileIndex].getGroundZOffset() <= MAX_ZPOS_FISH_SPAWN) {
            newFishChunk->mSpawnableTiles.setBit(tileIndex);
            ++newFishChunk->mTotalSpawnableTiles;
        }
    }

    if (timeSinceLastSpawned < MAX_DORMANCY_DURATION_SEC) {
        // TODO SPAWN FISHIES USING DORMANT DATA
    }
    else {
        // Fresh spawning
        const FishDef& codFish = mFishRepository.getFish("cod");
        const FishDef& rareFish = mFishRepository.getFish("rarefish");
        for (int j = 0; j < 70; ++j) {
            trySpawnFish(*chunk.getTileContainer(), *newFishChunk, codFish);
        }
        for (int j = 0; j < 15; ++j) {
            trySpawnFish(*chunk.getTileContainer(), *newFishChunk, rareFish);
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
    dormantChunk.mPopulations = std::move(chunk.mPopulations);
}

void FishEcosystem::makeUnDormant(FishChunk& chunk, DormantFishChunk& dormantChunk) {
    chunk.mPopulations = std::move(dormantChunk.mPopulations);
}

bool FishEcosystem::trySpawnFish(const TileContainer& container, FishChunk& chunk, const FishDef& fishDef) {
    TileIndex randomTile = Random::getCachedRandom() % CHUNK_SIZE;
    if (chunk.mSpawnableTiles.getBit(randomTile)) {

        entt::registry& registry = mWorld.getECS().mRegistry;
        const entt::entity newEntity = registry.create();
        assert(newEntity != INVALID_ENTITY);

        FishComponent& newFish = registry.emplace<FishComponent>(newEntity);
        PositionComponent& positionComponent = registry.emplace<PositionComponent>(newEntity);
        registry.emplace<VelocityComponent>(newEntity);
        registry.emplace<YawPitchComponent>(newEntity);
        registry.emplace<FishAIComponent>(newEntity);

        newFish.mFishId = fishDef.mId;
        newFish.mResidingChunk = chunk.mChunkID;
        const f32 groundZ = container.getTileAt(randomTile).getGroundZOffset();
        // Make sure this is actually underwater
        if (groundZ >= -FISH_COLLIDE_RADIUS) return false;
        const f32 randZPos = -FISH_COLLIDE_RADIUS + (groundZ + FISH_COLLIDE_RADIUS) * Random::xorshf96f();
        positionComponent.mPosition = f32v3(chunk.mWorldPos.x + randomTile % CHUNK_WIDTH, chunk.mWorldPos.y + randomTile / CHUNK_WIDTH, randZPos);
        chunk.mFish.emplace_back(newEntity);
    }
    return false;
}

void FishEcosystem::updateActiveFish() {
    PROFILE_FUNCTION();

    const f32 RENDER_DISTANCE_SQ = SQ(sDebugOptions.mFishRenderDistance);
    const f32v2 loadCenter = mWorld.getLoadCenter();

    if (mRenderStateManager) {

        entt::registry& registry = mWorld.getECS().mRegistry;

        FishChunkRenderStateMap& chunkMap = mRenderStateManager->getRenderStateForUpdate();
        chunkMap.clear();
        chunkMap.reserve(mActiveFishChunks.size());
        // Update fish chunks and cells
        for (auto&& activeFishChunkIter : mActiveFishChunks) {
            FishChunk& fishChunk = *activeFishChunkIter.second;
            if (glm::distance2(loadCenter, fishChunk.getWorldCenterF()) < RENDER_DISTANCE_SQ) {
                const ChunkID chunkId = activeFishChunkIter.first;
                TileContainer& container = *mWorld.getChunkGrid().getChunk(chunkId).getTileContainer();
                FishRenderState& renderState = chunkMap[container.getId()];
                // Update render state and update fish
                fishChunk.mInUpdateRange = true;
                renderState.mFish.resize(fishChunk.mFish.size());
                renderState.mChunkCenter = f32v2(fishChunk.mWorldPos) + f32v2(HALF_CHUNK_WIDTH);
                for (size_t j = 0; j < fishChunk.mFish.size(); ++j) {
                    entt::entity fishEntity = fishChunk.mFish[j];
                    FishRenderData& fishData = renderState.mFish[j];
                    FishComponent& fishCmp = registry.get<FishComponent>(fishEntity);
                    PositionComponent& positionCmp = registry.get<PositionComponent>(fishEntity);
                    YawPitchComponent& yawPitchCmp = registry.get<YawPitchComponent>(fishEntity);
                    fishData.mFishId = fishCmp.mFishId;
                    fishData.pos = positionCmp.mPosition;
                    fishData.yawPitch = yawPitchCmp.mYawPitch;

                    // Update fish
                    updateFish(registry, fishEntity, container, fishChunk, fishCmp, positionCmp, yawPitchCmp);
                }
            }
            else {
                fishChunk.mInUpdateRange = false;
            }
        }

        mRenderStateManager->finishUpdating();
    }
    else {
        // Dedicated server implementation
        assert(false);
    }
}

void FishEcosystem::updateFish(entt::registry& registry, entt::entity entity, const TileContainer& container, FishChunk& fishChunk, FishComponent& fish, PositionComponent& position, YawPitchComponent& yawPitch) {

    constexpr f32 PATH_SUCCESS_DISTANCE_SQ = SQ(0.1f);

    FishAIComponent& ai = registry.get<FishAIComponent>(entity);
    VelocityComponent& velocity = registry.get<VelocityComponent>(entity);

    // Update motion
    position.mPosition += velocity.mVelocity * mElapsedSec;

    constexpr auto getNewMovePosition = [](const TileContainer& container, FishChunk& fishChunk, FishComponent& fish, const PositionComponent& position, FishAIComponent& ai) {
        constexpr int MOVE_OFFSETS[8][2] = {
           {-1, -1},
           { 0, -1},
           {1, -1},
           {-1, 0},
           {1, 0},
           {-1, 1},
           {0, 1},
           {1, 1},
        };

        // New move position
        i32v2 chunkOffset = i32v2(position.mPosition) - fishChunk.mWorldPos;
        chunkOffset = glm::clamp(chunkOffset, 0, CHUNK_WIDTH - 1);
        int randomDir = Random::getCachedRandom() % 8;
        chunkOffset.x += MOVE_OFFSETS[randomDir][0];
        chunkOffset.y += MOVE_OFFSETS[randomDir][1];
        if (chunkOffset.x >= 0 && chunkOffset.y >= 0 &&
            chunkOffset.x < CHUNK_WIDTH && chunkOffset.y < CHUNK_WIDTH) {
            const f32v2 targetXY((f32)fishChunk.mWorldPos.x + (f32)chunkOffset.x + 0.5f, (f32)fishChunk.mWorldPos.y + (f32)chunkOffset.y + 0.5f);
            f32 targetZ = position.mPosition.z + Random::xorshf96f() * 2.0f - 1.0f;
            // Clamp to water
            const f32 groundZ = container.getTileAt(chunkOffset.y * CHUNK_WIDTH + chunkOffset.x).getGroundZOffset();
            if (groundZ <= -FISH_COLLIDE_RADIUS) {
                targetZ = glm::clamp(targetZ, groundZ + FISH_COLLIDE_RADIUS, -FISH_COLLIDE_RADIUS);
                ai.mTargetPosition = f32v3(targetXY.x, targetXY.y, targetZ);
                ai.mAIState = FishAIState::MovingToPoint;
            }
        }
    };

    switch (ai.mAIState) {
        case FishAIState::Idle: {
            // Drag
            velocity.mVelocity *= MathUtil::dragForceWithDeltaTime(0.7f, mElapsedSec);
            if (Random::getCachedRandomf() <= 0.01f) {
                getNewMovePosition(container, fishChunk, fish, position, ai);
            }
            
            break;
        }
        case FishAIState::MovingToPoint: {

            // Continue move chance
            if (Random::getCachedRandomf() <= 0.005f) {
                getNewMovePosition(container, fishChunk, fish, position, ai);
            }

            const f32v3 offsetToTarget = ai.mTargetPosition - position.mPosition;
            const f32 distSq = glm::length2(offsetToTarget);
            if (distSq <= PATH_SUCCESS_DISTANCE_SQ) {
                ai.mAIState = FishAIState::Idle;
            }
            else {
                constexpr f32 ROTATION_SPEED = 3.0f;
                constexpr f32 MAX_SPEED = 1.0f;
                const f32v3 targetVelocity = (offsetToTarget / sqrt(distSq)) * MAX_SPEED;
                velocity.mVelocity = MathUtil::lerpWithDeltaTime(velocity.mVelocity, targetVelocity, 0.9f, mElapsedSec);
                YawPitchComponent& yawPitch = registry.get<YawPitchComponent>(entity);
                yawPitch.mYaw = MathUtil::rotateYawToTarget(yawPitch.mYaw, std::atan2(velocity.mVelocity.x, velocity.mVelocity.y), ROTATION_SPEED * mElapsedSec);
            }
            break;
        }
        case FishAIState::PeckBobber: {
            FishingComponent& followFishingCmp = registry.get<FishingComponent>(ai.mFollowTarget);
            ai.mTargetPosition = followFishingCmp.mBobberPosition;
            const f32v3 offsetToTarget = ai.mTargetPosition - position.mPosition;
            const f32 distSq = glm::length2(offsetToTarget);
            if (distSq <= PATH_SUCCESS_DISTANCE_SQ) {
                if (ai.mPeckCountRemaining == 0) {
                    ai.mAIState = FishAIState::GrabBobber;
                    DebugRenderer::drawWireQuadThreadSafe(position.mPosition, f32v2(0.5f), color::Green, 60);
                }
                else {
                    DebugRenderer::drawWireQuadThreadSafe(position.mPosition, f32v2(0.5f), color::Red, 60);
                    ai.mAIState = FishAIState::PeckCooldown;
                    ai.mPeckCooldownRemaining = Random::xorshf96f() * (MAX_PECK_COOLDOWN_TIME - MIN_PECK_COOLDOWN_TIME) + MIN_PECK_COOLDOWN_TIME;
                    --ai.mPeckCountRemaining;
                }
            }
            else {
                constexpr f32 ROTATION_SPEED = 3.0f;
                constexpr f32 MAX_SPEED = 1.3f;
                const f32v3 targetVelocity = (offsetToTarget / sqrt(distSq)) * MAX_SPEED;
                velocity.mVelocity = MathUtil::lerpWithDeltaTime(velocity.mVelocity, targetVelocity, 0.9f, mElapsedSec);
                YawPitchComponent& yawPitch = registry.get<YawPitchComponent>(entity);
                yawPitch.mYaw = MathUtil::rotateYawToTarget(yawPitch.mYaw, -std::atan2(velocity.mVelocity.y, velocity.mVelocity.x) - M_PI_2f, ROTATION_SPEED * mElapsedSec);
            }
            break;
        }
        case FishAIState::PeckCooldown: {
            FishingComponent& followFishingCmp = registry.get<FishingComponent>(ai.mFollowTarget);
            ai.mPeckCooldownRemaining -= mElapsedSec;
            if (ai.mPeckCooldownRemaining < 0.0f) {
                ai.mAIState = FishAIState::PeckBobber;
            }
            else {
                // Float away slowly
                constexpr f32 MAX_SPEED = 1.0f;
                const f32v3 backwardsDir = MathUtil::directionFromYaw3D(yawPitch.mYaw);
                velocity.mVelocity = MathUtil::lerpWithDeltaTime(velocity.mVelocity, backwardsDir * MAX_SPEED, 0.95f, mElapsedSec);
            }
            break;
        }
        case FishAIState::GrabBobber: {
            FishingComponent& followFishingCmp = registry.get<FishingComponent>(ai.mFollowTarget);
            ai.mTargetPosition = followFishingCmp.mBobberPosition;
            position.mPosition = ai.mTargetPosition;
            break;
        }
        case FishAIState::OnFishingLine: {
            FishingComponent& followFishingCmp = registry.get<FishingComponent>(ai.mFollowTarget);
            ai.mTargetPosition = followFishingCmp.mBobberPosition;
            position.mPosition = ai.mTargetPosition;
            break;
        }
        default:
            assert(false);
        
    }
    static_assert(e_count(FishAIState) == 6);
    //fish.mPosition.x += (Random::getCachedRandomf() * 2.0f - 1.0f) * 0.1f;
    //fish.mPosition.y += (Random::getCachedRandomf() * 2.0f - 1.0f) * 0.1f;
}
