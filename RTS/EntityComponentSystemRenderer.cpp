#include "stdafx.h"
#include "EntityComponentSystemRenderer.h"
#include "EntityComponentSystem.h"
#include "Camera2D.h"
#include "EntityComponentSystem.h"
#include "World.h"

#include "rendering/CharacterRenderer.h"

#include <Vorb/utils.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/graphics/DepthState.h>

EntityComponentSystemRenderer::EntityComponentSystemRenderer(vg::TextureCache& textureCache, const EntityComponentSystem& system, const World& tileGrid)
	: mSpriteBatch(std::make_unique<vg::SpriteBatch>())
	, mSystem(system)
	, mTextureCache(textureCache)
	, mTileGrid(tileGrid) {
	// TODO: Render thread assert?
	mCircleTexture = mTextureCache.addTexture("data/textures/circle_dir.png");
	mSpriteBatch->init();
}

void EntityComponentSystemRenderer::renderPhysicsDebug(const Camera2D& camera) const {
	mSpriteBatch->begin();
	const PhysicsComponentTable& components = mSystem.getPhysicsComponents();
	for (auto&& it = components.cbegin(); it != components.cend(); ++it) {
		const PhysicsComponent& cmp = it->second;
		// TODO: 3D???
		mSpriteBatch->draw(mCircleTexture.id, cmp.getPosition() - cmp.mCollisionRadius, f32v2(cmp.mCollisionRadius * 2.0f), color4(1.0f, 0.0f, 0.0f));
	}

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix());
}

void EntityComponentSystemRenderer::renderSimpleSprites(const Camera2D& camera) const {
	mSpriteBatch->begin();

	const PhysicsComponentTable& physicsComponents = mSystem.getPhysicsComponents();
	const SimpleSpriteComponentTable& components = mSystem.getSimpleSpriteComponents();
	for (auto&& it = components.cbegin(); it != components.cend(); ++it) {
		const SimpleSpriteComponent& cmp = it->second;
		// TODO: 3D???
		const PhysicsComponent& physCmp = physicsComponents.get(cmp.physicsComponent);
		const f32 rotation = atan2(physCmp.mDir.y, physCmp.mDir.x);
		color4 color;
		color.lerp(cmp.color, color4(1.0f, 0.0f, 0.0f, 1.0f), cmp.hitFlash);
		mSpriteBatch->draw(mCircleTexture.id, nullptr, nullptr, physCmp.getPosition(), f32v2(0.5f), cmp.dims, rotation, cmp.color);
	}

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix());
}

void EntityComponentSystemRenderer::renderCharacterModels(const Camera2D& camera) {
	mSpriteBatch->begin();

	const PhysicsComponentTable& physicsComponents = mSystem.getPhysicsComponents();
	const CharacterModelComponentTable& components = mSystem.getCharacterModelComponents();
	for (auto&& it = components.cbegin(); it != components.cend(); ++it) {
		const CharacterModelComponent& cmp = it->second;
		const PhysicsComponent& physCmp = physicsComponents.get(cmp.mPhysicsComponent);
		// TODO: Common?
		const f32 rotation = atan2(physCmp.mDir.y, physCmp.mDir.x);
		CharacterRenderer::render(*mSpriteBatch, cmp.mModel, physCmp.getPosition(), rotation);
	}

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix(), nullptr, &vg::DepthState::FULL);
}
