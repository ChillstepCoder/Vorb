#include "stdafx.h"
#include "EntityComponentSystemRenderer.h"
#include "ecs/EntityComponentSystem.h"
#include "Camera2D.h"
#include "World.h"

#include "ResourceManager.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/LightRenderer.h"

#include <Vorb/utils.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/graphics/DepthState.h>

EntityComponentSystemRenderer::EntityComponentSystemRenderer(ResourceManager& resourceManager, const World& world)
	: mSpriteBatch(std::make_unique<vg::SpriteBatch>())
	, mSystem(world.getECS())
	, mResourceManager(resourceManager)
	, mWorld(world) {
	// TODO: Render thread assert?
	mCircleTexture = resourceManager.getTextureCache().addTexture("data/textures/circle_dir.png");
	mSpriteBatch->init();
}

void EntityComponentSystemRenderer::renderPhysicsDebug(const Camera2D& camera) const {
	mSpriteBatch->begin();

	auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent>().each([this](auto& cmp) {
		// TODO: 3D???
		mSpriteBatch->draw(mCircleTexture.id, cmp.getXYPosition() - cmp.mCollisionRadius, f32v2(cmp.mCollisionRadius * 2.0f), color4(1.0f, 0.0f, 0.0f));
	});

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix());
}

void EntityComponentSystemRenderer::renderSimpleSprites(const Camera2D& camera) const {
	mSpriteBatch->begin();

    auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent, SimpleSpriteComponent>().each([this](auto& physCmp, auto& spriteCmp) {
		const f32 rotation = atan2(physCmp.mDir.y, physCmp.mDir.x);
		color4 color;
		color.lerp(spriteCmp.mColor, color4(1.0f, 0.0f, 0.0f, 1.0f), spriteCmp.mHitFlash);
		mSpriteBatch->draw(mCircleTexture.id, nullptr, nullptr, physCmp.getXYPosition(), f32v2(0.5f), spriteCmp.mDims, rotation, spriteCmp.mColor, 0.05f);
	});

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix(), nullptr, &vg::DepthState::FULL);
}

void EntityComponentSystemRenderer::renderCharacterModels(const Camera2D& camera, const vg::DepthState& depthState, f32 alpha, f32 frameAlpha) {
	mSpriteBatch->begin();

	std::cout << frameAlpha << " \n";

    auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent, CharacterModelComponent>().each([this, alpha, frameAlpha](auto& physCmp, auto& modelCmp) {
		// TODO: Common?
		const f32 rotation = atan2(physCmp.mDir.y, physCmp.mDir.x);
		const f32v2& prevXY = physCmp.mPrevXYPosition;
		const f32v2& nextXY = physCmp.getXYPosition();
		f32v2 interpolatedXY;
		interpolatedXY.x = vmath::lerp(prevXY.x, nextXY.x, frameAlpha);
        interpolatedXY.y = vmath::lerp(prevXY.y, nextXY.y, frameAlpha);
		f32 interpolatedZ = vmath::lerp(physCmp.mPrevZPosition, physCmp.getZPosition(), frameAlpha);
		CharacterRenderer::render(*mSpriteBatch, modelCmp.mModel, interpolatedXY, interpolatedZ, rotation, alpha);
	});

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix(), nullptr, &depthState);
}

void EntityComponentSystemRenderer::renderDynamicLightComponents(const Camera2D& camera, const LightRenderer& lightRenderer) {
    auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent, DynamicLightComponent>().each([&](auto& physCmp, auto& lightCmp) {
		f32v2 pos = physCmp.getXYPosition();
		pos.y += physCmp.getZPosition() * Z_TO_XY_RATIO;
		lightRenderer.RenderLight(pos, lightCmp.mLightData, camera);
	});
}
