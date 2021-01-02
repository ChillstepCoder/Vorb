#pragma once
#include "rendering/CharacterModel.h"

#include <Vorb/graphics/SpriteBatch.h>


class CharacterRenderer {
public:
	static void render(vg::SpriteBatch& sb, const CharacterModel& model, const f32v2& xyPos, f32 zPos, float angle, float alpha);
};

//  TODO: This is temp af
extern vg::Texture sShadowTexture;

