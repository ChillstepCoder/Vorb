#pragma once

class World;


#include "world/IChunkGrid.h"

#include "definitions/FishDef.h"

typedef FlatMap<AssetID, int> FishPopulationMap;

template <typename T>
class RenderStateManager;

class TileContainer;
struct PositionComponent;
struct YawPitchComponent;

struct FishPopulation {
    AssetID mFishId;
    int mNumFish;
};

enum class FishAIState : ui8 {
    Idle,
    MovingToPoint,
    PeckBobber,
    PeckCooldown,
    GrabBobber,
    OnFishingLine,
    Caught,
    COUNT
};

struct FishAIComponent {
    union {
        TimePoint mTimeTargetReached = TimePoint::max();
        f32 mPeckCooldownRemaining;
        f32 mTimeUntilCaughtFinished;
    };
    f32v3 mTargetPosition = f32v3(0.0f);
    FishAIState mAIState = FishAIState::Idle;
    entt::entity mFollowTarget = INVALID_ENTITY; // TODO: Listen for destruction of this entity
    int mPeckCountRemaining = 0;
};

struct FishComponent {
    AssetID mFishId = INVALID_ASSET_ID; // TODO: Compress FishID?
    ChunkID mResidingChunk = INVALID_CHUNK_ID;
    f32v2 mAngularSpeed = f32v2(0.0f); // TODO: Quantized to i16?
    f32 mAnimationTime = 0.0f; // TODO: Quantized to i16?
};

struct FishChunk {
    std::vector<entt::entity> mFish; // LOD rendered
    i32v2 mWorldPos;
    TileContainerID mContainerID;
    ChunkID mChunkID;
    BitArray mSpawnableTiles;
    int mTotalSpawnableTiles = 0; // can derive max population and population pressure from this
    bool mInUpdateRange = false; // When false, we do not need to render fish

    f32v2 getWorldCenterF() const { return f32v2(mWorldPos.x + HALF_CHUNK_WIDTH, mWorldPos.y + HALF_CHUNK_WIDTH); }
};
typedef std::unique_ptr<FishChunk> FishChunkPtr;

struct DormantFishChunk {
    TimePoint mUnloadedTime; // TODO: GameTime for load/save
};

// TODO: Rename FishRenderState
struct FishRenderData {
    AssetID mFishId;
    f32v3 pos;
    f32v2 yawPitch;
    f32 scale;
    f32 turn;
    f32 time;
};

// TODO: Rename  FishChunkRenderState
struct FishRenderState {
    std::vector<FishRenderData> mFish;
    f32v2 mChunkCenter;
};

typedef UnorderedFlatMap<ChunkID, FishRenderState> FishChunkRenderStateMap;

class FishEcosystem
{
    friend class FishRenderer;
public:
    FishEcosystem(World& world);
    ~FishEcosystem();

    void tickGameThread(f32 elapsedSec);

    void initChunkFish(Chunk& chunk);

    RenderStateManager<FishChunkRenderStateMap>& getRenderStateManager() const { assert(mRenderStateManager); return *mRenderStateManager; }

    entt::entity getClosestIdleFishToPoint(f32v3 point, f32 maxRange) const;
    void removeFish(entt::entity fishEntity);
    void setFishFollowBobber(entt::entity fishEntity, entt::entity followTarget);
    void clearFishFollowTarget(entt::entity fishEntity);
    void setFishHooked(entt::entity fishEntity, entt::entity followTarget);
    void setFishCaught(entt::entity fishEntity, entt::entity catcher);
private:
    void initEventHandlers();
    void disposeChunkFish(Chunk& chunk);
    void makeDormant(FishChunk& chunk, DormantFishChunk& dormantChunk);
    void makeUnDormant(FishChunk& chunk, DormantFishChunk& dormantChunk);
    bool trySpawnFish(const TileContainer& container, FishChunk& fishChunk, const FishDef& fishDef);
    void updateActiveFish();
    bool updateFish(entt::registry& registry, entt::entity entity, const TileContainer& container, FishChunk& fishChunk, FishComponent& fish, PositionComponent& position, YawPitchComponent& yawPitch);

    void addTrackedFishPopulation(AssetID fishId, ChunkID chunkId);
    void removeTrackedFishPopulation(AssetID fishId, ChunkID chunkId);

    UnorderedFlatMap<ChunkID, FishChunkPtr> mActiveFishChunks;

    std::mutex mDormantMutex; // TODO: Everything is on game thread???
    UnorderedFlatMap<ChunkID, DormantFishChunk> mDormantFishChunks;

    std::mutex mGenerationMutex; // TODO: Everything is on game thread???
    UnorderedFlatMap<ChunkID, FishChunkPtr> mGeneratedFishChunks;

    // Population tracking
    FishPopulationMap mTotalFishPopulation;
    UnorderedFlatMap<ChunkID, FishPopulationMap> mChunkFishPopulations;

    // Asset handles
    UnorderedFlatMap<AssetID, AssetHandlePtr<FishDef>> mFishAssetHandles;

    World& mWorld;

    ChunkGridListeners mChunkGridEventListeners;

    // Very slow ticking for dormancy updates
    TickingTimer mDormancyUpdateTicker = TickingTimer(1000.0f);
    f32 mElapsedSec = 0.0f;

    std::unique_ptr<RenderStateManager<FishChunkRenderStateMap>> mRenderStateManager;
};

