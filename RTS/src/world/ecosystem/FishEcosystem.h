#pragma once

class IWorld;

// TODO: MOVE TO FISHREPOSITORY
typedef ui32 FishID;

#include <boost/container/flat_map.hpp>

#include "world/IChunkGrid.h"

typedef boost::container::flat_map<FishID, int> FishPopulationMap;

template <typename T>
class RenderStateManager;

class TileContainer;
struct FishDef;
class FishRepository;
struct PositionComponent;
struct YawPitchComponent;

struct FishPopulation {
    FishID mFishId;
    int mNumFish;
};

enum class FishAIState : ui8 {
    Idle,
    MovingToPoint,
    PeckBobber,
    PeckCooldown,
    GrabBobber,
    OnFishingLine,
    COUNT
};

struct FishAIComponent {
    union {
        TimePoint mTimeTargetReached = TimePoint::max();
        f32 mPeckCooldownRemaining;
    };
    f32v3 mTargetPosition = f32v3(0.0f);
    FishAIState mAIState = FishAIState::Idle;
    entt::entity mFollowTarget = INVALID_ENTITY; // TODO: Listen for destruction of this entity
    int mPeckCountRemaining = 0;
};

struct FishComponent {
    FishID mFishId = INVALID_FISH_ID; // TODO: Compress FishID?
    ChunkID mResidingChunk = INVALID_CHUNK_ID;
};

struct FishChunk {
    std::vector<entt::entity> mFish; // LOD rendered
    i32v2 mWorldPos;
    TileContainerID mContainerID;
    ChunkID mChunkID;
    FishPopulationMap mPopulations;
    BitArray mSpawnableTiles;
    int mTotalSpawnableTiles = 0; // can derive max population and population pressure from this
    bool mInUpdateRange = false; // When false, we do not need to render fish

    f32v2 getWorldCenterF() const { return f32v2(mWorldPos.x + HALF_CHUNK_WIDTH, mWorldPos.y + HALF_CHUNK_WIDTH); }
};
typedef std::unique_ptr<FishChunk> FishChunkPtr;

struct DormantFishChunk {
    FishPopulationMap mPopulations;
    TimePoint mUnloadedTime; // TODO: GameTime for load/save
};

// TODO: Rename FishRenderState
struct FishRenderData {
    FishID mFishId;
    f32v3 pos;
    f32v2 yawPitch;
};

// TODO: Rename  FishChunkRenderState
struct FishRenderState {
    std::vector<FishRenderData> mFish;
    f32v2 mChunkCenter;
};

typedef boost::container::flat_map<ChunkID, FishRenderState> FishChunkRenderStateMap;

class FishEcosystem
{
    friend class FishRenderer;
public:
    FishEcosystem(IWorld& world);
    ~FishEcosystem();

    void tickGameThread(f32 elapsedSec);

    void initChunkFish(Chunk& chunk);

    RenderStateManager<FishChunkRenderStateMap>& getRenderStateManager() const { assert(mRenderStateManager); return *mRenderStateManager; }

    entt::entity getClosestIdleFishToPoint(f32v3 point, f32 maxRange) const;
    void removeFish(entt::entity fishEntity);
    void setFishFollowBobber(entt::entity fishEntity, entt::entity followTarget);
    void clearFishFollowTarget(entt::entity fishEntity);
private:
    void initEventHandlers();
    void disposeChunkFish(Chunk& chunk);
    void makeDormant(FishChunk& chunk, DormantFishChunk& dormantChunk);
    void makeUnDormant(FishChunk& chunk, DormantFishChunk& dormantChunk);
    bool trySpawnFish(const TileContainer& container, FishChunk& fishChunk, const FishDef& fishDef);
    void updateActiveFish();
    void updateFish(entt::registry& registry, entt::entity entity, const TileContainer& container, FishChunk& fishChunk, FishComponent& fish, PositionComponent& position, YawPitchComponent& yawPitch);

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
    f32 mElapsedSec = 0.0f;

    std::unique_ptr<RenderStateManager<FishChunkRenderStateMap>> mRenderStateManager;
};

