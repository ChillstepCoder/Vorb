#include "stdafx.h"
#include "EntityComponentSystemRenderer.h"
#include "ecs/IEntityComponentSystem.h"
#include "ecs/business/BusinessComponent.h"
#include "ecs/component/OwnershipComponent.h"
#include "camera/Camera3D.h"
#include "city/CityPlot.h"
#include "world/IWorld.h"

#include "resources/ResourceManager.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/LightRenderer.h"
#include "debugging/DebugRenderer.h"
#include "options/DebugOptions.h"
#include "rendering/CityDebugRenderer.h"

#include <Vorb/utils.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/graphics/DepthState.h>

EntityComponentSystemRenderer::EntityComponentSystemRenderer()
	: mSpriteBatch(std::make_unique<vg::SpriteBatch>()) {
	// TODO: Render thread assert?
	vg::TextureCache& textureCache = Services::ResourceManager::ref().getTextureCache();
    mCircleTexture = textureCache.addTexture("data/textures/circle_dir.png");
    mSquareTexture = textureCache.addTexture("data/textures/square.png");
	mSpriteBatch->init();
}

EntityComponentSystemRenderer::~EntityComponentSystemRenderer()
{

}

void EntityComponentSystemRenderer::renderBusinessDebug(const Camera3D& camera) const {

	if (++mFrameCount <= mFramesPerDebugDraw) {
		return;
	}
	mFrameCount = 0;

    int i = 0;
    // Blueprint debug
    if (sDebugOptions.mBlueprintDebug) {
        auto& ecs = sMainGameWorld->getECS();

		auto view = ecs.mRegistry.view<BusinessBuildComponent>();
		for (auto entity : view) {
			BuildingBlueprint* bp = ecs.mRegistry.get<BusinessBuildComponent>(entity).mCurrentBlueprint;
			if (bp) {
				CityDebugRenderer::renderBlueprintDebug(*bp, mFramesPerDebugDraw);
			}
		}
	}
}

void EntityComponentSystemRenderer::renderDynamicLightComponents(const Camera3D& camera, const LightRenderer& lightRenderer) {

    auto& ecs = sMainGameWorld->getECS();
	// TODO: 3D
	//ecs.mRegistry.view<PhysicsComponent, DynamicLightComponent>().each([&](auto& physCmp, auto& lightCmp) {
	//	assert(false);
	//	//f32v2 pos = physCmp.getXYPosition();
	//	//pos.y += physCmp.getZPosition() * 0.75f; // Magic z_to_xy_ratio
	//	//lightRenderer.RenderLight(pos, lightCmp.mLightData, camera);
	//});
}

void EntityComponentSystemRenderer::renderInteractUI(const Camera3D& camera) const {
    mSpriteBatch->begin();

    //auto& ecs = sWorld->getECS();

	const f32v2 fullSize(1.0f, 0.25f);
	const f32v2 offset(fullSize.x * -0.5f, 1.0f);
 //   ecs.mRegistry.view<PhysicsComponent, TimedTileInteractComponent>().each([this, fullSize, offset](auto& physCmp, auto& interactCmp) {
	//	//// Background
 // //      mSpriteBatch->draw(mSquareTexture.id, nullptr, nullptr, physCmp.getXYPosition() + offset, f32v2(0.0f), fullSize, 0.0f /*rot*/, color4(1.0f, 0.0f, 0.0f, 0.5f), 1.7f);
	//	//// Foreground fill
	//	//const f32v2 fillSize(fullSize.x * interactCmp.mProgress, fullSize.y);
	//	//mSpriteBatch->draw(mSquareTexture.id, nullptr, nullptr, physCmp.getXYPosition() + offset, f32v2(0.0f), fillSize, 0.0f /*rot*/, color4(0.0f, 1.0f, 0.0f, 1.0f), 1.71f);
	//	assert(false);
	//});
	

    mSpriteBatch->end();
    mSpriteBatch->render(f32m4(1.0f), camera.getVPMatrix(), nullptr, &vg::DepthState::FULL);
}
