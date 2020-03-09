#pragma once
#include "stdafx.h"

#include "rendering/CharacterModel.h"

#include <Vorb/graphics/SpriteBatch.h>

class CharacterRenderer {
public:
	static void render(vg::SpriteBatch& sb, const CharacterModel& model, const f32v2& pos, float angle);
};

