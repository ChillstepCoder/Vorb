#include "stdafx.h"
#include "EntityFactory.h"

#include "actor/HumanActorFactory.h"
#include "actor/PlayerActorFactory.h"
#include "actor/UndeadActorFactory.h"

#include "EntityComponentSystem.h"

#include <Vorb/io/Path.h>

EntityFactory::EntityFactory(EntityComponentSystem& ecs, ResourceManager& resourceManager) {
    mHumanActorFactory = std::make_unique<HumanActorFactory>(ecs.mRegistry, ecs.mWorld, resourceManager);
    mPlayerActorFactory = std::make_unique<PlayerActorFactory>(ecs.mRegistry, ecs.mWorld, resourceManager);
    mUndeadActorFactory = std::make_unique<UndeadActorFactory>(ecs.mRegistry, ecs.mWorld, resourceManager);
}

EntityFactory::~EntityFactory()
{

}

entt::entity EntityFactory::createEntity(const f32v2& position, EntityType type) {
    switch (type) {
        case EntityType::PLAYER:
            return mPlayerActorFactory->createActor(position,
                vio::Path("data/textures/circle_dir.png"),
                vio::Path("")
            );
            break;
        case EntityType::UNDEAD:
            return mUndeadActorFactory->createActor(
                position,
                vio::Path("data/textures/circle_dir.png"),
                vio::Path("")
            );
            break;
        case EntityType::HUMAN:
            return mHumanActorFactory->createActor(
                position,
                vio::Path("data/textures/circle_dir.png"),
                vio::Path("")
            );
            break;
        default:
            assert(false);
            break;
    }
    return (entt::entity)0;
}
