#pragma once

class Camera3D;

struct CharacterControlComponent;

struct PlayerControlComponent {
	ui16 mPlayerControlFlags = 0;
};

class PlayerControlSystem {
public:
	PlayerControlSystem();
	void update(entt::registry& registry, f32 cameraYaw);

private:
	void updateComponent(entt::entity entity, PlayerControlComponent& playerControlCmp, CharacterControlComponent& characterControlCmp, entt::registry& registry, f32 cameraYaw);
};