#pragma once

class IWorld;
class PhysicsComponent;
struct CharacterControlComponent;

enum class FishingComponentState {
    Initializing,
    Casting,
    Casted,
    Fishing,
    NPCMinigame,
    RemotePlayerMinigame,
    LocalPlayerMinigame,
    Success, // >= here means done
    Fail,
    COUNT
};

struct FishingComponent {
    FishingComponentState mState = FishingComponentState::Casting;
    f32v3 mBobberPosition = f32v3(0.0f);
    f32v3 mTargetPosition = f32v3(0.0f); // Cast target
    f32v3 mBobberVelocity = f32v3(0.0f);
    f32 mCastCharge = 0.0f;
    bool mIsCastInputPressed = false;
    entt::entity mTargetFish = INVALID_ENTITY;

    bool isDone() const { return mState >= FishingComponentState::Success; }
};

class FishingComponentSystem {
public:
    void update(IWorld& world, entt::registry& registry);
private:
    void updateFishing(IWorld& world, entt::registry& registry, entt::entity, FishingComponent& fishCmp, PhysicsComponent& physCmp, CharacterControlComponent& characterControlCmp);

    f32 mTimeStep = 0.0f;
};