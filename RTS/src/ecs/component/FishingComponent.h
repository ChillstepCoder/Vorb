#pragma once

class IWorld;
class PhysicsComponent;
struct CharacterControlComponent;

enum class FishingComponentState {
    Casting,
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
    bool mIsCastInputPressed = false;
    f32 mCastCharge = 0.0f;

    bool isDone() const { return mState >= FishingComponentState::Success; }
};

class FishingComponentSystem {
public:
    void update(IWorld& world, entt::registry& registry);
private:
    void updateFishing(IWorld& world, entt::registry& registry, FishingComponent& fishCmp, PhysicsComponent& physCmp, CharacterControlComponent& characterControlCmp);
};