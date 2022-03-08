#pragma once

struct SpriteData;
class SpriteRepository;

#include <Vorb/graphics/Texture.h>

#include "definitions/ModelDef.h"
#include "definitions/AnimMachineDef.h"

enum CharacterModelTextureIndex {
	CHARACTER_MODEL_TEXTURE_FRONT = 0,
	CHARACTER_MODEL_TEXTURE_SIDE  = 1,
	CHARACTER_MODEL_TEXTURE_BACK  = 2
};



// TODO: File name
struct CharacterModelComponent {
	const ModelDef* mModel = nullptr;
	AnimState mAnimState;
};