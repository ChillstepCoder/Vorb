#pragma once

struct EntityDistInfo {
	float dist;
	int quadrant; // Used for special arc queries
};

typedef std::pair<EntityDistInfo, entt::entity> EntityDistSortKey;

enum ActorTypes : ui8 {
	ACTORTYPE_NONE = 0,
	ACTORTYPE_UNDEAD = 1 << 0,
	ACTORTYPE_HUMAN = 1 << 1,
	ACTORTYPE_CORPSE = 1 << 1,
	ACTORTYPE_ANY = 0xFF
};

typedef ui8 ActorTypesMask;