#pragma once

#include <Vorb/ecs/Entity.h>

struct EntityDistInfo {
	float dist;
	int quadrant; // Used for special arc queries
};

typedef std::pair<EntityDistInfo, vecs::EntityID> EntityDistSortKey;

enum ActorTypes : ui32 {
	ACTORTYPE_NONE = 0,
	ACTORTYPE_UNDEAD = 1 << 0,
	ACTORTYPE_HUMAN = 1 << 1,
	ACTORTYPE_CORPSE = 1 << 1,
	ACTORTYPE_ANY = ~0u
};

typedef ui32 ActorTypesMask;