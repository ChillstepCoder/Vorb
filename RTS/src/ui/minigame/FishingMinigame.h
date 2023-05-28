#pragma once

#include <Vorb/graphics/SpriteBatch.h>

class MaterialShader;
struct MeshGpuData;

struct FishingMinigameFishData {
    f32 mMaxSpeed = 5.0f; // Maximum speed the circle can move
    f32v2 mAcceleration = f32v2(0.5f, 0.25f); // How quickly the fish accelerates in X and Y
    f32 mFishDrag = 0.02f;
    f32 mJerkChance = 0.01f; // Chance to instantly change direction and speed
    f32 mJerkIntensity = 4.0f; // How much the speed can change on a jerk
    f32v2 mJerkCooldownVarianceSec = f32v2(1.0f, 2.0f);
    f32 mRadius = 0.5f; // Radius of the fish target circle [0, 1]
    f32 mSteeringIntensity = 1.0f; // How strongly it steers around vs going straight
    f32 mWallBouncyness = 0.5f; // Wall collision elasticity
    f32 mGravity = 0.1f; // Gravity to the top of the circle
    f32 mCenterMagnitism = 0.05f; // Gravity to the center of the circle
    f32 mFailAngle = 45.0f;
    // TODO:
    f32 mStaminaDepleteRate = 1.0f; // How quickly its stamina bar depletes
    f32 mStaminaRechargeRate = 1.0f; // How quickly its stamina bar refills
    f32 mOutOfStaminaPowerMult = 0.5f; // How much weaker it is when its stamina is gone
    f32 mFishDamageRate = 1.0f; // How fast the success meter increases
    f32 mPlayerDamageRate = 0.0f; // How fast the success meter depletes

    // TODO: Move to PlayerData
    f32 mSuccessAngle = 30.0f;
    f32 mPlayerMaxSpeed = 5.0f;
    f32 mPlayerAcceleration = 2.0f;
    f32 mPlayerDrag = 0.1f;
    f32 mPlayerWallBouncyness = 0.1f;
    f32v2 mPlayerStickyness = f32v2(0.01f, 0.4f);
    f32 mPlayerStrength = 1.0f;
};

struct FishData {
    ItemID mFishItem;
    FishingMinigameFishData mMinigameData;
};

enum class MinigameResultType {
    InProgress,
    Fail,
    Success,
    COUNT
};

struct FishingMinigameResult {
    MinigameResultType result = MinigameResultType::InProgress;
    ItemID fishItem = INVALID_ITEM_ID;
    int fishQuality = 0; // TODO: What is this?
};

struct MinigameDebugTextFloater {
    f32v2 mPosition;
    TimePoint mSpawnTime;
    color4 mColor;
    const char* mText;
};

class FishingMinigame {
public:
    FishingMinigame(const FishData& fishData);
    ~FishingMinigame();

    FishingMinigameResult updateAndRender(const f32v2 screenResolution);

private:
    MinigameResultType update();
    void render();

    void updateFishPosition();
    void updatePlayerPosition();
    void fishLifeLost();
    void playerLifeLost();
    void win();
    void lose();

    void renderDebugFloaters();
    void addDebugFloater(const char* text, f32v2 position, color4 color);


    f32 mFishRadius;
    const f32 mPlayerRadius;

    const FishData& mFishData;
    vg::SpriteBatch mSpriteBatch;
    VGTexture mTextureCircle;

    static constexpr int UPDATE_RATE_MS = 40;
    TickingTimer mTickingTimer = TickingTimer(UPDATE_RATE_MS);

    // Minigame data
    f32v2 mPlayerPosition = f32v3(0.0f);
    f32v2 mPlayerVelocity = f32v2(0.0f);
    f32v2 mFishPosition = f32v2(0.0f);
    f32v2 mFishVelocity = f32v2(0.0f);
    TimePoint mLastFishLifeLostTime = {};
    TimePoint mLastJerkTime = {};
    f32 mCurrentJerkCooldown = 0.0f;
    f32v2 mCurrentScreenResolution = f32v2(0.0f);
    bool mIsPlayerTouchingFish = false;
    int mPlayerLivesLeft = 3;
    int mFishLivesLeft = 3;
    MinigameResultType mStatus = MinigameResultType::InProgress;

    TimePoint mEndTransitionTimeStart = TimePoint::max();

    std::vector<MinigameDebugTextFloater> mDebugFloaters;

    const MaterialShader* mArenaShader = nullptr;

};

