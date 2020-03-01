#pragma once
#include "stdafx.h"

#include <Vorb/ecs/Entity.h>
#include <Vorb/ecs/ComponentTable.hpp>

class EntityComponentSystem;
class TileGrid;

enum class PlayerControlFlags : ui16 {
	SPRINTING = 1 << 0
};

struct PlayerControlComponent {
	ui16 mPlayerControlFlags = 0;
};

class PlayerControlComponentTable : public vecs::ComponentTable<PlayerControlComponent> {
public:
	static const std::string& NAME;

	void update(EntityComponentSystem& ecs, TileGrid& world);
};