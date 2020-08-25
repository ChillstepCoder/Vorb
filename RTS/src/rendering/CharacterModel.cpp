#include "stdafx.h"
#include "CharacterModel.h"

#include <Vorb/graphics/TextureCache.h>

constexpr const char* CHARACTER_TEXTURE_ROOT = "data/textures/character/";
constexpr const char* FRONT_SUFFIX = "_front.png";
constexpr const char* SIDE_SUFFIX = "_side.png";
constexpr const char* BACK_SUFFIX = "_back.png";

const std::string& CharacterModelComponentTable::NAME = "character_model";

void loadTexturesForPart(vg::TextureCache& textureCache, vg::Texture textures[3], const std::string& name) {
	std::string path;
	vg::Texture texture;
	// Front
	path = CHARACTER_TEXTURE_ROOT + name + FRONT_SUFFIX;
	texture = textureCache.addTexture(path.c_str());
	assert(texture.id);
	textures[CHARACTER_MODEL_TEXTURE_FRONT] = std::move(texture);
	// Side
	path = CHARACTER_TEXTURE_ROOT + name + SIDE_SUFFIX;
	texture = textureCache.addTexture(path.c_str());
	assert(texture.id);
	textures[CHARACTER_MODEL_TEXTURE_SIDE] = std::move(texture);
	// Back
	path = CHARACTER_TEXTURE_ROOT + name + BACK_SUFFIX;
	texture = textureCache.addTexture(path.c_str());
	assert(texture.id);
	textures[CHARACTER_MODEL_TEXTURE_BACK] = std::move(texture);
}

void CharacterModel::load(vg::TextureCache& textureCache, const std::string& face, const std::string& body, const std::string& hair) {
	loadTexturesForPart(textureCache, mFaceTextures, face);
	loadTexturesForPart(textureCache, mBodyTextures, body);
	loadTexturesForPart(textureCache, mHairTextures, hair);
}
