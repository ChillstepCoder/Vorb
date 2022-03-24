#pragma once

class World;
class Camera3D;

struct PlayerControlComponent {
	ui16 mPlayerControlFlags = 0;
};

class PlayerControlSystem {
public:
	void update(entt::registry& registry, const Camera3D& camera);
};