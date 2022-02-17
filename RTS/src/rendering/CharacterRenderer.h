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

// TODO: Cutout rendering - see pathfinder wrath of the righteous
class CharacterRenderer {
public:
	CharacterRenderer(const MaterialManager& materialManager);
	~CharacterRenderer();
	void addModel(const Camera3D& camera, const CharacterModel& model, const f32v3& position, float angle, float alpha);
	void renderBatch(const Camera3D& camera, const MaterialRenderer& materialRenderer);

private:
	void buildPart(BillboardMesh& mesh, const f32v3& rootPos, const f32v2& offset, const SpriteData& spriteData, bool shouldFlip, float width, float alpha);

    const Material* mMaterial;
    std::unique_ptr<BillboardMesh> mMesh;
};

//  TODO: This is temp af
//extern vg::Texture sShadowTexture;

