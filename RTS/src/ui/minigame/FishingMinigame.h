#pragma once

struct FishingMinigameFishData {
    f32 mMaxSpeed = 1.0f; // Maximum speed the circle can move
    f32 mAcceleration = 1.0f; // How quickly the fish accelerates
    f32 mJerkChance = 0.01f; // Chance to instantly change direction and speed
    f32 mJerkIntensity = 0.1f; // How much the speed can change on a jerk
    f32 mRadius = 1.0f; // Radius of the fish target circle
    f32 mStaminaDepleteRate = 1.0f; // How quickly its stamina bar depletes
    f32 mStaminaRechargeRate = 1.0f; // How quickly its stamina bar refills
    f32 mOutOfStaminaPowerMult = 0.5f; // How much weaker it is when its stamina is gone
    f32 mSteeringIntensity = 1.0f; // How strongly it steers around vs going straight
    f32 mWallBouncyness = 0.5f; // Wall collision elasticity
    f32 mGravity = 0.0f; // Gravity to the bottom of the circle
    f32 mCenterMagnitism = 0.0f; // Gravity to the center of the circle
    f32 mFishDamageRate = 1.0f; // How fast the success meter increases
    f32 mPlayerDamageRate = 0.0f; // How fast the success meter depletes
};

struct FishData {
    ItemID mFishItem;
    FishingMinigameFishData mMinigameData;
};

class FishingMinigame {
public:
    FishingMinigame(const FishData& fishData);

    void updateAndRender();

private:
    const FishData& mFishData;
};

