#include "PlayerActorFactory.h"

#include "EntityComponentSystem.h"

PlayerActorFactory::PlayerActorFactory(EntityComponentSystem& ecs, vg::TextureCache& textureCache)
	: HumanActorFactory(ecs, textureCache) {
}

vecs::EntityID PlayerActorFactory::createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) {
	vecs::EntityID newEntity = HumanActorFactory::createActor(position, texturePath, definitionFile);

	auto& playerComp = static_cast<EntityComponentSystem&>(mEcs).addPlayerControlComponent(newEntity).second;
	UNUSED(playerComp);

	mEcs.deleteComponent(mEcs.mSoldierAITable.getID(), newEntity);

	// Player combat
	//auto& combatComp = mEcs.getCombatComponentFromEntity(newEntity);

	// model
	auto& characterModelComp = static_cast<EntityComponentSystem&>(mEcs).addCharacterModelComponent(newEntity).second;
	characterModelComp.mModel.load(mTextureCache, "face/female/Female_Average_Wide", "body/thin", "hair/longB");
	characterModelComp.mPhysicsComponent = static_cast<EntityComponentSystem&>(mEcs).mPhysicsTable.getComponentID(newEntity);

	return newEntity;
}
