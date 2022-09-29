#pragma once

class Camera3D;

struct CharacterControlComponent;

struct PlayerControlComponent {
	ui16 mPlayerControlFlags = 0;
};

class PlayerControlSystem {
public:
	PlayerControlSystem();
	void update(entt::registry& registry, const Camera3D& camera);

private:
	void updateComponent(entt::entity entity, PlayerControlComponent& controlCmp, CharacterControlComponent& motionCmp, entt::registry& registry, const Camera3D& camera);
};