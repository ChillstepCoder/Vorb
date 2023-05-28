#pragma once

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

struct FishDef {
    ItemID mItem;
    FishID mId;
    FishingMinigameFishData mMinigameData;
};