#pragma once

#include <Vorb/graphics/Texture.h>

DECL_VG(class SpriteBatch)
DECL_VG(class DepthState)

class Camera3D;
class PhysicsSystem;
class CharacterRenderer;
class EntityComponentSystem;
class MaterialRenderer;
class World;
class LightRenderer;

class EntityComponentSystemRenderer {
public:
	EntityComponentSystemRenderer(const World& world);
    void renderPhysicsDebug(const Camera3D& camera) const;
    void renderBusinessDebug(const Camera3D& camera) const;
	void renderSimpleSprites(const Camera3D& camera) const;
	void renderCharacterModels(CharacterRenderer& renderer, MaterialRenderer& materialRenderer, const Camera3D& camera, f32 frameAlpha, f32 elapsedSec);
	void renderDynamicLightComponents(const Camera3D& camera, const LightRenderer& lightRenderer);
	void renderInteractUI(const Camera3D& camera) const;

private:
	std::unique_ptr<vg::SpriteBatch> mSpriteBatch;
    vg::Texture mCircleTexture;
    vg::Texture mSquareTexture;
	const EntityComponentSystem& mSystem;
	const World& mWorld;
};

