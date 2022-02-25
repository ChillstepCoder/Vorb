#include "stdafx.h"
#include "EntityComponentSystemRenderer.h"
#include "ecs/EntityComponentSystem.h"
#include "ecs/business/BusinessComponent.h"
#include "ecs/component/OwnershipComponent.h"
#include "camera/Camera3D.h"
#include "city/CityPlot.h"
#include "World.h"

#include "ResourceManager.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/LightRenderer.h"
#include "DebugRenderer.h"
#include "rendering/CityDebugRenderer.h"

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

void EntityComponentSystemRenderer::renderPhysicsDebug(const Camera3D& camera) const {

	auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent>().each([this](auto& cmp) {
        DebugRenderer::drawCircle(cmp.getPosition(), cmp.mCollisionRadius, color4(1.0f, 0.0f, 0.0f));
        //DebugRenderer::drawCircle(cmp.getPosition() + f32v3(0.0f, 0.0f, cmp.mCollisionHeight), cmp.mCollisionRadius, color4(0.5f, 0.0f, 0.0f));
	});
}

void EntityComponentSystemRenderer::renderBusinessDebug(const Camera3D& camera) const {

    int i = 0;

    auto& ecs = mWorld.getECS();
    ecs.mRegistry.view<OwnershipComponent>().each([ &i](auto& cmp) {
		color4 color((i * 120) % 255, 255 - (i * 60) % 255, (i * 72) % 255, 255);
		for (CityPlot* plot : cmp.mOwnedPlots) {
			if (plot->mPendingBlueprint) {
				CityDebugRenderer::renderBlueprintDebug(*plot->mPendingBlueprint, &color);
			}
		}
		++i;
    });
}

void EntityComponentSystemRenderer::renderSimpleSprites(const Camera3D& camera) const {
	mSpriteBatch->begin();

    auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent, SimpleSpriteComponent>().each([this](auto& physCmp, auto& spriteCmp) {
		const f32 rotation = atan2(physCmp.mDir.y, physCmp.mDir.x);
		color4 color;
		color.lerp(spriteCmp.mColor, color4(1.0f, 0.0f, 0.0f, 1.0f), spriteCmp.mHitFlash);
		mSpriteBatch->draw(mCircleTexture.id, nullptr, nullptr, physCmp.getXYPosition(), f32v2(0.5f), spriteCmp.mDims, rotation, spriteCmp.mColor, 0.05f);
	});

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getVPMatrix(), nullptr, &vg::DepthState::FULL);
}

void EntityComponentSystemRenderer::renderCharacterModels(CharacterRenderer& renderer, MaterialRenderer& materialRenderer, const Camera3D& camera, f32 alpha, f32 frameAlpha) {
	// TODO: This should not be using spritebatch. It should use a custom 
	// renderer so that it can add screen depth like the world shaders do
	
    auto& ecs = mWorld.getECS();
	ecs.mRegistry.view<PhysicsComponent, CharacterModelComponent>().each([&](auto& physCmp, auto& modelCmp) {
		// TODO: Common?
		const f32 rotation = atan2(physCmp.mDir.y, physCmp.mDir.x);
		f32v2 interpolatedXY = physCmp.getXYInterpolated(frameAlpha);
		f32 interpolatedZ = physCmp.getZInterpolated(frameAlpha);
		renderer.addModel(camera, modelCmp.mModel, f32v3(interpolatedXY.x, interpolatedXY.y, interpolatedZ), rotation, alpha, materialRenderer);
	});
	renderer.renderBatch(camera, materialRenderer);
}

void EntityComponentSystemRenderer::renderDynamicLightComponents(const Camera3D& camera, const LightRenderer& lightRenderer) {
    auto& ecs = mWorld.getECS();
	// TODO: 3D
	ecs.mRegistry.view<PhysicsComponent, DynamicLightComponent>().each([&](auto& physCmp, auto& lightCmp) {
		f32v2 pos = physCmp.getXYPosition();
		pos.y += physCmp.getZPosition() * 0.75f; // Magic z_to_xy_ratio
		lightRenderer.RenderLight(pos, lightCmp.mLightData, camera);
	});
}

void EntityComponentSystemRenderer::renderInteractUI(const Camera3D& camera) const {
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
    mSpriteBatch->render(f32m4(1.0f), camera.getVPMatrix(), nullptr, &vg::DepthState::FULL);
}
