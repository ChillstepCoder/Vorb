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

// TODO: Cutout rendering - see pathfinder wrath of the righteous
class CharacterRenderer {
public:
	CharacterRenderer(const MaterialManager& materialManager, const ModelRepository& modelRepo);
	~CharacterRenderer();
	void addModel(const Camera3D& camera, const CharacterModel& model, const f32v3& position, float angle, float alpha, const MaterialRenderer& materialRenderer);
	void renderBatch(const Camera3D& camera, const MaterialRenderer& materialRenderer);

private:

    const Material* mMaterial;
	const ModelRepository& mModelRepo;
};

//  TODO: This is temp af
//extern vg::Texture sShadowTexture;

