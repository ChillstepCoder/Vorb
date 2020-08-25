#pragma once
#include <Vorb/ecs/ComponentTable.hpp>

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
};

// TODO: File name
struct CharacterModelComponent {
	CharacterModel mModel;
	vecs::ComponentID mPhysicsComponent = 0;
};

class CharacterModelComponentTable : public vecs::ComponentTable<CharacterModelComponent> {
public:
	static const std::string& NAME;
};



