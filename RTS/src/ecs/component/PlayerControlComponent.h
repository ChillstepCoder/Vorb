#pragma once

struct ClientECSData;
class World;

enum class PlayerControlFlags : ui16 {
	SPRINTING = 1 << 0
};

struct PlayerControlComponent {
	ui16 mPlayerControlFlags = 0;
};

class PlayerControlSystem {
public:
	void update(entt::registry& registry, World& world, const ClientECSData& clientData);
};