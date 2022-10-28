#pragma once
#include "rendering/CharacterModel.h"

class MaterialRenderer;
class PhysicsComponent;
class Material;
struct CharacterModelComponent;
struct CharacterControlComponent;
class Camera3D;

class CharacterRenderer {
public:
	CharacterRenderer();
	~CharacterRenderer();

	void renderModel(const Camera3D& camera, CharacterModelComponent& cmp, const PhysicsComponent& physCmp, const CharacterControlComponent& motionCmp, f32 elapsedSec, f32 frameAlpha, const MaterialRenderer& materialRenderer);
private:

    const Material* mMaterial;
};

//  TODO: This is temp af
//extern vg::Texture sShadowTexture;

