#include "stdafx.h"
#include "ecs/component/SimpleSpriteComponent.h"

const std::string& SimpleSpriteComponentTable::NAME = "simplesprite";

// TODO: dont really like this
void SimpleSpriteComponentTable::update() {
	// Update components
	for (auto&& cmp : *this) {
		if (isValid(cmp)) {
			cmp.second.hitFlash *= 0.95f;
		}
	}
}

