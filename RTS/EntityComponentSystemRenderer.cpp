#include "stdafx.h"
#include "EntityComponentSystemRenderer.h"
#include "EntityComponentSystem.h"
#include "Camera2D.h"
#include "EntityComponentSystem.h"
#include "TileGrid.h"

#include <Vorb/utils.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>

EntityComponentSystemRenderer::EntityComponentSystemRenderer(vg::TextureCache& textureCache, const EntityComponentSystem& system, const TileGrid& tileGrid)
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
		const f32v2 position = mTileGrid.convertWorldCoordToScreen(cmp.getPosition() - cmp.mCollisionRadius);
		mSpriteBatch->draw(mCircleTexture.id, position, f32v2(cmp.mCollisionRadius * 2.0f), color4(1.0f, 0.0f, 0.0f));
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
		const f32v2 position = mTileGrid.convertWorldCoordToScreen(physCmp.getPosition());
		const f32v2 convertedDir = mTileGrid.convertWorldCoordToScreen(physCmp.mDir);
		const f32 rotation = atan2(convertedDir.y, convertedDir.x);
		color4 color;
		color.lerp(cmp.color, color4(1.0f, 0.0f, 0.0f, 1.0f), cmp.hitFlash);
		mSpriteBatch->draw(mCircleTexture.id, nullptr, nullptr, position, f32v2(0.5f), cmp.dims, rotation, cmp.color);
	}

	mSpriteBatch->end();
	mSpriteBatch->render(f32m4(1.0f), camera.getCameraMatrix());
}
