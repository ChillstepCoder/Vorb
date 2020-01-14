#pragma once
#include "stdafx.h"

#include <Vorb/ecs/Entity.h>
#include <Vorb/ecs/ComponentTable.hpp>


class EntityComponentSystem;

struct PlayerControlComponent {
};

class PlayerControlComponentTable : public vecs::ComponentTable<PlayerControlComponent> {
public:
	static const std::string& NAME;

	void update(EntityComponentSystem& ecs);
};