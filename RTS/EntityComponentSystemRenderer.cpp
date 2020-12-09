#include "stdafx.h"
#include "EntityComponentSystemRenderer.h"
#include "EntityComponentSystem.h"
#include "Camera2D.h"
#include "EntityComponentSystem.h"
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
		mSpriteBatch->draw(mCircleTexture.id, cmp.getPosition() - cmp.mCollisionRadius, f32v2(cmp.mCollisionRadius * 2.0f), color4(1.0f, 0.0f, 0.0f));
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
		mSpriteBatch->draw(mCircleTexture.id, nullptr, nullptr, physCmp.getPosition(), f32v2(0.5f), spriteCmp.mDims, rotation, spriteCmp.mColor, 0.05f);
	});

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix(), nullptr, &vg::DepthState::FULL);
}

void EntityComponentSystemRenderer::renderCharacterModels(const Camera2D& camera) {
	mSpriteBatch->begin();

    auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent, CharacterModelComponent>().each([this](auto& physCmp, auto& modelCmp) {
		// TODO: Common?
		const f32 rotation = atan2(physCmp.mDir.y, physCmp.mDir.x);
		CharacterRenderer::render(*mSpriteBatch, modelCmp.mModel, physCmp.getPosition(), rotation);
	});

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix(), nullptr, &vg::DepthState::FULL);
}

void EntityComponentSystemRenderer::renderDynamicLightComponents(const Camera2D& camera, const LightRenderer& lightRenderer) {
    auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent, DynamicLightComponent>().each([&](auto& physCmp, auto& lightCmp) {
		lightRenderer.RenderLight(physCmp.getPosition(), lightCmp.mLightData, camera);
	});
}
