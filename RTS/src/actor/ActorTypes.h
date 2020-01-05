#pragma once
#include "stdafx.h"

enum ActorTypes : ui32 {
	ACTORTYPE_NONE = 0,
	ACTORTYPE_UNDEAD = 1 << 0,
	ACTORTYPE_HUMAN = 1 << 1
};

typedef ui32 ActorTypesMask;