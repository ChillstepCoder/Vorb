#pragma once

struct CharacterControlComponent;
class IWorld;

enum class PlayerControlFlags : ui8 {
};

struct PlayerControlComponent {
	BitFlags<PlayerControlFlags> mPlayerControlFlags;
	ui8 mInputLockCount = 0; // TODO: LockHandle RAII so we never leak locks
};

class PlayerControlSystem {
public:
	PlayerControlSystem();
	void update(IWorld& world, entt::registry& registry, f32 cameraYaw);

private:
	void updateComponent(IWorld& world, entt::entity entity, PlayerControlComponent& playerControlCmp, CharacterControlComponent& characterControlCmp, entt::registry& registry, f32 cameraYaw);
};