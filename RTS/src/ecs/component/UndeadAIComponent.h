#pragma once
#include "stdafx.h"

#include "actor/ActorTypes.h"
#include <Vorb/ecs/ComponentTable.hpp>

class EntityComponentSystem;
class TileGrid;

struct UndeadAIComponent {
	int mState = 0;
};

class UndeadAIComponentTable : public vecs::ComponentTable<UndeadAIComponent> {
public:
	static const std::string& NAME;

	void update(EntityComponentSystem& ecs, TileGrid& world);
};