#pragma once

#include <Vorb/graphics/Texture.h>

DECL_VG(class SpriteBatch)
DECL_VG(class DepthState)

class Camera3D;
class PhysicsSystem;
class CharacterRenderer;
class EntityComponentSystem;
class MaterialRenderer;
class ResourceManager;
class World;
class LightRenderer;

class EntityComponentSystemRenderer {
public:
	EntityComponentSystemRenderer(ResourceManager& resourceManager, const World& world);
	void renderPhysicsDebug(const Camera3D& camera) const;
	void renderSimpleSprites(const Camera3D& camera) const;
	void renderCharacterModels(CharacterRenderer& renderer, MaterialRenderer& materialRenderer, const Camera3D& camera, f32 alpha, f32 frameAlpha);
	void renderDynamicLightComponents(const Camera3D& camera, const LightRenderer& lightRenderer);
	void renderInteractUI(const Camera3D& camera) const;

private:
	std::unique_ptr<vg::SpriteBatch> mSpriteBatch;
	ResourceManager& mResourceManager;
    vg::Texture mCircleTexture;
    vg::Texture mSquareTexture;
	const EntityComponentSystem& mSystem;
	const World& mWorld;
};

