#pragma once

#include "interact/SelectedObjectData.h"

struct CharacterControlComponent;
class World;

struct Camera3DGameThreadData;
struct PlayerInputs;

enum class PlayerControlFlags : ui8 {
};

struct PlayerControlComponent {
	BitFlags<PlayerControlFlags> mPlayerControlFlags;
	ui8 mInputLockCount = 0; // TODO: LockHandle RAII so we never leak locks
	f32 mInteractDuration = 0.0f;
	SelectedObjectData mSelectedObjectData;
};

class PlayerControlSystem {
public:
	PlayerControlSystem(World& world, entt::registry& registry);
	void update(const Camera3DGameThreadData& cameraData, f32 elapsedSec);

private:
	void updateComponent(entt::entity entity, PlayerControlComponent& playerControlCmp, CharacterControlComponent& characterControlCmp, const Camera3DGameThreadData& cameraData, f32 elapsedSec);
	void updateSelection(entt::entity entity, PlayerControlComponent& playerControlCmp, const Camera3DGameThreadData& cameraData, const PlayerInputs& inputs, f32 elapsedSec);

	World& mWorld;
	entt::registry& mRegistry;
};