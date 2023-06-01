#pragma once

#include <Vorb/graphics/SpriteBatch.h>

#include "definitions/FishDef.h"

class MaterialShader;
struct MeshGpuData;

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
    FishingMinigame(const FishDef& fishData);
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

    const FishDef& mFishDef;
    vg::SpriteBatch mSpriteBatch;
    VGTexture mTextureCircle;
    VGTexture mTextureBackground;

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

