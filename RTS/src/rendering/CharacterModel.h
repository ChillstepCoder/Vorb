#pragma once

DECL_VG(class TextureCache);

#include <Vorb/graphics/Texture.h>

enum CharacterModelTextureIndex {
	CHARACTER_MODEL_TEXTURE_FRONT = 0,
	CHARACTER_MODEL_TEXTURE_SIDE  = 1,
	CHARACTER_MODEL_TEXTURE_BACK  = 2
};


class CharacterModel {
public:
	void load(vg::TextureCache& textureCache, const std::string& face, const std::string& body, const std::string& hair);

	vg::Texture mFaceTextures[3];
	vg::Texture mHairTextures[3];
	vg::Texture mBodyTextures[3];
	bool isMale = false;
};

// TODO: File name
struct CharacterModelComponent {
	CharacterModel mModel;
};