#pragma once

struct FishingMinigameFishData {
    f32 mMaxSpeed = 265.0f; // Maximum speed the circle can move
    f32v2 mAcceleration = f32v2(450.0f, 350.0f); // How quickly the fish accelerates in X and Y
    f32 mFishDrag = 0.03f;
    f32 mJerkChance = 0.01f; // Chance to instantly change direction and speed
    f32 mJerkIntensity = 4.0f; // How much the speed can change on a jerk
    f32v2 mJerkCooldownVarianceSec = f32v2(1.0f, 2.0f);
    f32 mRadius = 0.7f; // Radius of the fish target circle [0, 1]
    f32 mSteeringIntensity = 1.0f; // How strongly it steers around vs going straight
    f32 mWallBouncyness = 0.5f; // Wall collision elasticity
    f32 mGravity = 80.0f; // Gravity to the top of the circle
    f32 mCenterMagnitism = 50.0f; // Gravity to the center of the circle
    f32 mFailAngle = 45.0f;
    int mParticleMaterial = 0;
    f32 mParticleScale = 0.56f;
    // TODO:
    f32 mStaminaDepleteRate = 1.0f; // How quickly its stamina bar depletes
    f32 mStaminaRechargeRate = 1.0f; // How quickly its stamina bar refills
    f32 mOutOfStaminaPowerMult = 0.5f; // How much weaker it is when its stamina is gone
    f32 mFishDamageRate = 1.0f; // How fast the success meter increases
    f32 mPlayerDamageRate = 0.0f; // How fast the success meter depletes

    // TODO: Move to PlayerData
    f32 mSuccessAngle = 30.0f;
    f32 mPlayerMaxSpeed = 200.0f;
    f32 mPlayerAcceleration = 2600.0f;
    f32 mPlayerDrag = 0.99f;
    f32 mPlayerWallBouncyness = 0.25f;
    f32v2 mPlayerStickyness = f32v2(0.99f, 0.999f);
    f32 mPlayerStrength = 150.0f;
};

class FishDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(FishDef);

    StrToken mItemName;
    StrToken mModelName;
    ItemID mItemId;
    ModelID mModelId;
    FishingMinigameFishData mMinigameData; // TODO: Yml
};
SERIALIZABLE_SIMPLE(FishDef,
    make_field(o.mItemName, "item"sv),
    make_field(o.mModelName, "model"sv)
);