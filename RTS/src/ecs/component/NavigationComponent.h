#pragma once
#include "stdafx.h"

#include "actor/ActorTypes.h"
#include <Vorb/ecs/ComponentTable.hpp>

class EntityComponentSystem;

struct NavigationComponent {
	// TODO Pathfinding
	// TODO Entity handles
	f32v2 mTargetPos = f32v2(0.0f);
	float speed = 1.0f;
	bool mHasTarget = false;
};

class NavigationComponentTable : public vecs::ComponentTable<NavigationComponent> {
public:
	static const std::string& NAME;

	void update(EntityComponentSystem& ecs);
};