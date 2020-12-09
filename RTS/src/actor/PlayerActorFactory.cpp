#include "stdafx.h"
#include "PlayerActorFactory.h"

#include "ResourceManager.h"

#include "EntityComponentSystem.h"

PlayerActorFactory::PlayerActorFactory(entt::registry& registry, World& world, ResourceManager& resourceManager)
	: HumanActorFactory(registry, world, resourceManager) {
}

entt::entity PlayerActorFactory::createActor(const f32v2& position, const vio::Path& texturePath, const vio::Path& definitionFile) {
	entt::entity newEntity = HumanActorFactory::createActor(position, texturePath, definitionFile);

	mRegistry.emplace<PlayerControlComponent>(newEntity);

	// Player combat
	//auto& combatComp = mEcs.getCombatComponentFromEntity(newEntity);

    // model
    auto& modelCmp = mRegistry.emplace<CharacterModelComponent>(newEntity);
	modelCmp.mModel.load(mResourceManager.getTextureCache(), "face/female/Female_Average_Wide", "body/thin", "hair/longB");

	return newEntity;
}
