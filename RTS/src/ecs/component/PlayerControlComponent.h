#pragma once

struct ClientECSData;
class World;

struct PlayerControlComponent {
	ui16 mPlayerControlFlags = 0;
};

class PlayerControlSystem {
public:
	void update(entt::registry& registry, const ClientECSData& clientData);
};