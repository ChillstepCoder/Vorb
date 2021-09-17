#pragma once
#include "rendering/CharacterModel.h"

#include <Vorb/graphics/SpriteBatch.h>

class MaterialRenderer;
class MaterialManager;
class SpriteData;
class Material;
class Camera3D;
struct BillboardVertex;

// TODO: Cutout rendering - see pathfinder wrath of the righteous
class CharacterRenderer {
public:
	CharacterRenderer(const MaterialManager& materialManager);
	void render(const Camera3D& camera, const MaterialRenderer& materialRenderer, const CharacterModel& model, const f32v3& position, float angle, float alpha);

private:
	void buildPart(std::vector<BillboardVertex>& vertexData, const f32v3& rootPos, const f32v2& offset, const SpriteData& spriteData, bool shouldFlip, float width, float depth, float alpha);

	const Material* mMaterial;
};

//  TODO: This is temp af
//extern vg::Texture sShadowTexture;

