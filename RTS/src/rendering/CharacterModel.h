#pragma once

class SpriteData;
class SpriteRepository;

#include <Vorb/graphics/Texture.h>

enum CharacterModelTextureIndex {
	CHARACTER_MODEL_TEXTURE_FRONT = 0,
	CHARACTER_MODEL_TEXTURE_SIDE  = 1,
	CHARACTER_MODEL_TEXTURE_BACK  = 2
};


class CharacterModel {
public:
	void load(SpriteRepository& spriteRepository, const std::string& face, const std::string& body, const std::string& hair);

	const SpriteData* mFaceSprites[3];
	const SpriteData* mHairSprites[3];
	const SpriteData* mBodySprites[3];
	bool isMale = false;
};

// TODO: File name
struct CharacterModelComponent {
	CharacterModel mModel;
};