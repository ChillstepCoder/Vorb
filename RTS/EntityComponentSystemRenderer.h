#pragma once
#include "Camera2D.h"

#include <Vorb/graphics/Texture.h>

DECL_VG(class SpriteBatch)

class Camera2D;
class PhysicsComponentTable;
class EntityComponentSystem;
class ResourceManager;
class World;
class LightRenderer;

class EntityComponentSystemRenderer {
public:
	EntityComponentSystemRenderer(ResourceManager& resourceManager, const World& world);
	void renderPhysicsDebug(const Camera2D& camera) const;
	void renderSimpleSprites(const Camera2D& camera) const;
	void renderCharacterModels(const Camera2D& camera);
	void renderDynamicLightComponents(const Camera2D& camera, const LightRenderer& lightRenderer);

private:
	std::unique_ptr<vg::SpriteBatch> mSpriteBatch;
	ResourceManager& mResourceManager;
	vg::Texture mCircleTexture;
	const EntityComponentSystem& mSystem;
	const World& mWorld;
};

