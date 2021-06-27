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
    mSquareTexture = resourceManager.getTextureCache().addTexture("data/textures/square.png");
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

    auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent, CharacterModelComponent>().each([this, alpha, frameAlpha](auto& physCmp, auto& modelCmp) {
		// TODO: Common?
		const f32 rotation = atan2(physCmp.mDir.y, physCmp.mDir.x);
		f32v2 interpolatedXY = physCmp.getXYInterpolated(frameAlpha);
		f32 interpolatedZ = physCmp.getZInterpolated(frameAlpha);
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

void EntityComponentSystemRenderer::renderInteractUI(const Camera2D& camera) const {
    mSpriteBatch->begin();

    auto& ecs = mWorld.getECS();

	const f32v2 fullSize(1.0f, 0.25f);
	const f32v2 offset(fullSize.x * -0.5f, 1.0f);
    ecs.mRegistry.view<PhysicsComponent, TimedTileInteractComponent>().each([this, fullSize, offset](auto& physCmp, auto& interactCmp) {
		// Background
        mSpriteBatch->draw(mSquareTexture.id, nullptr, nullptr, physCmp.getXYPosition() + offset, f32v2(0.0f), fullSize, 0.0f /*rot*/, color4(1.0f, 0.0f, 0.0f, 0.5f), 1.7f);
		// Foreground fill
		const f32v2 fillSize(fullSize.x * interactCmp.mProgress, fullSize.y);
		mSpriteBatch->draw(mSquareTexture.id, nullptr, nullptr, physCmp.getXYPosition() + offset, f32v2(0.0f), fillSize, 0.0f /*rot*/, color4(0.0f, 1.0f, 0.0f, 1.0f), 1.71f);
    });

    mSpriteBatch->end();
    mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix(), nullptr, &vg::DepthState::FULL);
}
