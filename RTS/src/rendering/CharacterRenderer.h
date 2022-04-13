#pragma once
#include "rendering/CharacterModel.h"

#include <Vorb/graphics/SpriteBatch.h>

class MaterialRenderer;
class MaterialManager;
struct SpriteData;
class Material;
class Camera3D;
struct BillboardVertex;
class BillboardMesh;
class ModelRepository;
struct CharacterModelComponent;
struct LocomotionComponent;
class PhysicsComponent;


// TODO: Cutout rendering - see pathfinder wrath of the righteous
class CharacterRenderer {
public:
	CharacterRenderer();
	~CharacterRenderer();

	void addModel(const Camera3D& camera, CharacterModelComponent& cmp, const PhysicsComponent& physCmp, const LocomotionComponent& motionCmp, f32 elapsedSec, f32 frameAlpha, const MaterialRenderer& materialRenderer);
    void renderBatch(const Camera3D& camera, const MaterialRenderer& materialRenderer);

private:

    const Material* mMaterial;
};

//  TODO: This is temp af
//extern vg::Texture sShadowTexture;

