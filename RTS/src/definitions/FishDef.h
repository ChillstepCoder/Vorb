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
SERIALIZABLE_IMGUI_CONTROLLED(FishingMinigameFishData,
    make_field(o.mMaxSpeed, "max_speed"sv),
    make_field(o.mAcceleration, "accel"sv),
    make_field(o.mFishDrag, "drag"sv),
    make_field(o.mJerkChance, "jerk_chance"sv),
    make_field(o.mJerkIntensity, "jerk_int"sv),
    make_field(o.mJerkCooldownVarianceSec, "jerk_cool"sv),
    make_field(o.mRadius, "radius"sv),
    make_field(o.mSteeringIntensity, "steer_int"sv),
    make_field(o.mWallBouncyness, "wall_bounce"sv),
    make_field(o.mGravity, "gravity"sv),
    make_field(o.mCenterMagnitism, "center_mag"sv),
    make_field(o.mFailAngle, "fail_angle"sv),
    make_field(o.mParticleMaterial, "particle_mat"sv),
    make_field(o.mParticleScale, "particle_scale"sv),
    make_field(o.mStaminaDepleteRate, "stam_deplete"sv),
    make_field(o.mStaminaRechargeRate, "stam_rech"sv),
    make_field(o.mOutOfStaminaPowerMult, "oos_power"sv),
    make_field(o.mFishDamageRate, "fish_damage"sv),
    make_field(o.mPlayerDamageRate, "player_damage"sv)
)

class FishDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(FishDef);

    SoftAssetReference mItemRef = AssetType::Item;
    SoftAssetReference mModelRef = AssetType::Model;
    ItemID mItemId = INVALID_ITEM_ID;
    ModelID mModelId = INVALID_ASSET_ID;
    FishingMinigameFishData mMinigameData; // TODO: Yml
};
SERIALIZABLE_IMGUI_CONTROLLED(FishDef,
    make_field(o.mItemRef, "item"sv),
    make_field(o.mModelRef, "model"sv),
    make_field(o.mMinigameData, "minigame"sv)
);