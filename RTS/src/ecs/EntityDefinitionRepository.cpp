#include "stdafx.h"
#include "EntityDefinitionRepository.h"

#include "entt/entity/storage.hpp"

#include "ecs/EntityComponentSystem.h"

#include "ecs/component/EntityDefinition.h"

#include "ResourceManager.h"

#include <Vorb/io/IOManager.h>

EntityDefinitionRepository::EntityDefinitionRepository(vio::IOManager& ioManager)
    : mIoManager(ioManager) {
}

EntityDefinitionRepository::~EntityDefinitionRepository()
{
}

void EntityDefinitionRepository::loadEntityDefinitionFile(const vio::Path& filePath)
{
    std::unique_ptr<EntityDefinition> entityDef = std::make_unique<EntityDefinition>();

    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
        keg::ReadContext& readContext = *((keg::ReadContext*)s);

        if (key == ComponentTypeStrings[enum_cast(ComponentTypes::CharacterModel)]) {
            entityDef->components.emplace_back(ComponentTypes::CharacterModel);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::Combat)]) {
            entityDef->components.emplace_back(ComponentTypes::Combat);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::Corpse)]) {
            entityDef->components.emplace_back(ComponentTypes::Corpse);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::DynamicLight)]) {
            entityDef->components.emplace_back(ComponentTypes::DynamicLight);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::Navigation)]) {
            entityDef->components.emplace_back(ComponentTypes::Navigation);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::Physics)]) {
            //auto& physics = mTemplateRegistry.emplace<PhysicsComponent>(templateEntity/*, mWorld, position, false*/);
            //physics.mQueryActorTypes = ACTORTYPE_HUMAN;
            //physics.addCollider(newEntity, ColliderShapes::SPHERE, SPRITE_RADIUS);
            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::Physics);
            keg::parse((ui8*)&fileData.physics, value, readContext, &KEG_GLOBAL_TYPE(PhysicsComponentDef));
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::PlayerControl)]) {
            entityDef->components.emplace_back(ComponentTypes::PlayerControl);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::Profession)]) {
            entityDef->components.emplace_back(ComponentTypes::Profession);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::SimpleSprite)]) {
            entityDef->components.emplace_back(ComponentTypes::SimpleSprite);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::SoldierAI)]) {
            entityDef->components.emplace_back(ComponentTypes::SoldierAI);
        }
        else if (key == ComponentTypeStrings[enum_cast(ComponentTypes::UndeadAI)]) {
            entityDef->components.emplace_back(ComponentTypes::UndeadAI);
        }
        else {
            pError("Tried to load invalid .entt component type \"" + key + "\"");
        }
        static_assert(enum_cast(ComponentTypes::COUNT) == 11, "Parse new component type");
        // Load data
        //BuildingDescription description
    }))) {
        //mTemplateEntities[filePath.getFileNameNoExtension()] = templateEntity;
        mEntityDefinitions[filePath.getFileNameNoExtension()] = std::move(entityDef);
    } else {
        // Failure case
        pError("Failed to parse entity file " + filePath.getString());
    }
}

const EntityDefinition& EntityDefinitionRepository::getDefinition(const nString& typeName)
{
    auto&& it = mEntityDefinitions.find(typeName);
    assert(it != mEntityDefinitions.end());
    return *it->second;
}
