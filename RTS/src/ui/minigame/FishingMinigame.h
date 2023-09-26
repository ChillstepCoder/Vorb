#pragma once

// TODO: REMOVE
#include <Vorb/graphics/SpriteBatch.h>

#include "ui/minigame/ILocalMinigame.h"
#include "definitions/FishDef.h"

#include "resources/asset/AssetHandleBundle.h"

class MaterialShaderDef;
class CPUParticleSystem;
struct MeshGpuData;


struct FishingMinigameResult {
    MinigameResultType result = MinigameResultType::InProgress;
    ItemID fishItem = INVALID_ITEM_ID;
    int fishQuality = 0; // TODO: What is this?
    int caughtChest = 0; // 1, 2, 3, 4
};

struct MinigameDebugTextFloater {
    f32v2 mPosition;
    TimePoint mSpawnTime;
    color4 mColor;
    const char* mText;
};

struct FishingMinigameGameThreadData {
    std::mutex mMutex;
    f32v2 mBobberOffset = f32v2(0.0f);
    int mTugOfWarValue = 0;
};

enum class FishingMinigameFlags : ui8 {
    DISABLE_DEBRIS = BIT(0),
    DISABLE_CHESTS = BIT(1)
};

class FishingMinigame : public ILocalMinigame {
public:
    FishingMinigame(const FishDef& fishData, OPT FishingMinigameGameThreadData* gameThreadData, std::function<void(FishingMinigameResult& result)> onFinished, BitFlags<FishingMinigameFlags> flags);
    ~FishingMinigame();

    VORB_NON_COPYABLE_BUT_MOVABLE(FishingMinigame);

    MinigameResult updateAndRender(const f32v2 screenResolution, f32 elapsedSec) override;
    void abort() override;

private:
    MinigameResultType update();
    void render(f32 elapsedSec);

    void initUIParticles();
    void initPlayerParticles();
    void initBlockerParticles();
    void initBubbleParticles();

    void updateFishPosition(f32 elapsedSec);
    void updatePlayerPosition(f32 elapsedSec);
    void updateChestPosition(f32 elapsedSec);
    void fishLifeLost();
    void playerLifeLost();
    void catchChest();
    void win();
    void lose();

    void renderDebugFloaters();
    void addDebugFloater(const char* text, f32v2 position, color4 color);


    f32 mFishRadius;
    const f32 mPlayerRadius;

    const FishDef& mFishDef;

    // UI Data
    std::unique_ptr<CPUParticleSystem> mUIParticleSystem;
    std::unique_ptr<CPUParticleSystem> mBackgroundParticleSystem;
    std::unique_ptr<CPUParticleSystem> mPlayerParticleSystem;
    std::unique_ptr<CPUParticleSystem> mBlockerParticleSystem;
    std::unique_ptr<CPUParticleSystem> mBubbleParticleSystem;
    ParticleID mArenaParticleID;
    ParticleID mPlayerParticleID;
    ParticleID mFishParticleID;
    ParticleID mChestParticleID = INVALID_PARTICLE_ID;
    std::vector<ParticleID> mBlockerParticles;


    vg::SpriteBatch mSpriteBatch; //TODO: REMOVE

    static constexpr int UPDATE_RATE_MS = 16; // TODO: NEED INTERPOLATION
    TickingTimer mTickingTimer = TickingTimer(UPDATE_RATE_MS, UPDATE_RATE_MS * 2);

    // Minigame data
    f32v2 mPlayerPosition = f32v3(0.0f);
    f32v2 mPlayerVelocity = f32v2(0.0f);
    f32v2 mFishPosition = f32v2(0.0f);
    f32v2 mFishVelocity = f32v2(0.0f);
    f32v2 mChestPosition = f32v2(0.0f);
    f32v2 mChestVelocity = f32v2(0.0f);
    bool mCaughtChest = false;
    ui32 mChestTier = 0; // 1, 2, 3, 4
    TimePoint mLastFishLifeLostTime = {};
    TimePoint mLastJerkTime = {};
    f32 mCurrentJerkCooldown = 0.0f;
    f32v2 mCurrentScreenResolution = f32v2(0.0f);
    bool mIsPlayerTouchingFish = false;
    bool mDidPlayerImpactBottom = false;
    int mTugOfWarValue = 0; // Positive is winning
    MinigameResultType mStatus = MinigameResultType::InProgress;

    BitFlags<FishingMinigameFlags> mFlags;

    // Optional, used to position bobber and fish in world
    FishingMinigameGameThreadData* mGameThreadData = nullptr;

    std::function<void(FishingMinigameResult& result)> mOnFinished;

    TimePoint mEndTransitionTimeStart = TimePoint::max();

    std::vector<MinigameDebugTextFloater> mDebugFloaters;

    const MaterialShaderDef* mArenaShader = nullptr;
    const MaterialShaderDef* mUIShader = nullptr;
    AssetHandleBundle mAssetHandles;
};

