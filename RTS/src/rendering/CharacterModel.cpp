#include "stdafx.h"
#include "CharacterModel.h"

#include <Vorb/graphics/TextureCache.h>

#include "rendering/SpriteRepository.h"

// TODO: Remove, this is used for the static sShadowTexture load
#include "CharacterRenderer.h"

//constexpr const char* CHARACTER_TEXTURE_ROOT = "data/textures/character/";
constexpr const char* FRONT_SUFFIX = "_front";
constexpr const char* SIDE_SUFFIX = "_side";
constexpr const char* BACK_SUFFIX = "_back";

void loadTexturesForPart(SpriteRepository& spriteRepository, const SpriteData* sprites[3], const std::string& name) {
	std::string path;
	const SpriteData* sprite;
	// Front
	path = /*CHARACTER_TEXTURE_ROOT + */name + FRONT_SUFFIX;
	sprite = &spriteRepository.getSprite(path.c_str());
	assert(sprite->isValid());
	sprites[CHARACTER_MODEL_TEXTURE_FRONT] = std::move(sprite);
	// Side
	path = /*CHARACTER_TEXTURE_ROOT + */name + SIDE_SUFFIX;
	sprite = &spriteRepository.getSprite(path.c_str());
	assert(sprite->isValid());
	sprites[CHARACTER_MODEL_TEXTURE_SIDE] = std::move(sprite);
	// Back
	path = /*CHARACTER_TEXTURE_ROOT +*/ name + BACK_SUFFIX;
	sprite = &spriteRepository.getSprite(path.c_str());
	assert(sprite->isValid());
	sprites[CHARACTER_MODEL_TEXTURE_BACK] = std::move(sprite);
}

void CharacterModel::load(SpriteRepository& spriteRepository, const std::string& face, const std::string& body, const std::string& hair) {

	loadTexturesForPart(spriteRepository, mFaceSprites, face);
	loadTexturesForPart(spriteRepository, mBodySprites, body);
	loadTexturesForPart(spriteRepository, mHairSprites, hair);
}
