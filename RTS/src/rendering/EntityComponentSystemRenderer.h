#pragma once
#include "camera/Camera2D.h"

#include <Vorb/graphics/Texture.h>

DECL_VG(class SpriteBatch)
DECL_VG(class DepthState)

class Camera3D;
class PhysicsSystem;
class EntityComponentSystem;
class ResourceManager;
class World;
class LightRenderer;

class EntityComponentSystemRenderer {
public:
	EntityComponentSystemRenderer(ResourceManager& resourceManager, const World& world);
	void renderPhysicsDebug(const Camera3D& camera) const;
	void renderSimpleSprites(const Camera3D& camera) const;
	void renderCharacterModels(const Camera3D& camera, const f32m4& vp, const vg::DepthState& depthState, f32 alpha, f32 frameAlpha);
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

