#pragma once

class IWorld;
class PhysicsComponent;
struct CharacterControlComponent;

enum class FishingComponentState {
    Initializing,
    Casting,
    Casted,
    Fishing,
    FishGrabbed,
    NPCMinigame,
    RemotePlayerMinigame,
    LocalPlayerMinigame,
    Success, // >= here means done
    Fail,
    COUNT
};

// TODO: Does it make sense to separate out player data?
struct FishingComponent {
    FishingComponentState mState = FishingComponentState::Casting;
    f32v3 mBobberPosition = f32v3(0.0f);
    f32v3 mTargetPosition = f32v3(0.0f); // Cast target
    f32v3 mBobberVelocity = f32v3(0.0f);
    f32 mCastCharge = 0.0f;
    bool mIsCastInputPressed = false;
    bool mIsLocalPlayer = false;
    entt::entity mTargetFish = INVALID_ENTITY;

    void onBobberGrabbed(entt::entity fishEntity);
    void onFishLost();
    bool isDone() const { return mState >= FishingComponentState::Success; }
};

class FishingComponentSystem {
public:
    FishingComponentSystem();
    ~FishingComponentSystem();

    void update(IWorld& world, entt::registry& registry);
private:
    void updateFishing(IWorld& world, entt::registry& registry, entt::entity, FishingComponent& fishCmp, PhysicsComponent& physCmp, CharacterControlComponent& characterControlCmp);

    f32 mTimeStep = 0.0f;

    // TODO: Instead have processInput() function, and have an input queue on this object which is filled by an input
    // thread, so the input thread can process at 144fps even when game thread and render thread are ticking slower.
    // - What does this give us? it is equivalent to checking an atomic bool, just cleaner
    std::atomic_bool mWasButtonPressed;
};