#include "stdafx.h"
#include "EntityDefinitionRepository.h"

#include "entt/entity/storage.hpp"

#include "ecs/IEntityComponentSystem.h"

#include "ecs/component/EntityDefinition.h"

#include "resources/ResourceManager.h"

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

        if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterModel)]) {
            entityDef->components.emplace_back(ComponentTypes::CharacterModel);
        } else if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterControl)]) {
            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::CharacterControl);
            fileData.characterControl = CharacterControlComponentDef(); // Default initialize
            keg::parse((ui8*)&fileData.characterDetails, value, readContext, &KEG_GLOBAL_TYPE(CharacterControlComponentDef));
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Combat)]) {
            entityDef->components.emplace_back(ComponentTypes::Combat);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Corpse)]) {
            entityDef->components.emplace_back(ComponentTypes::Corpse);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterDetails)]) {
            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::CharacterDetails);
            fileData.characterDetails = CharacterDetailsComponentDef(); // Default initialize
            keg::parse((ui8*)&fileData.characterDetails, value, readContext, &KEG_GLOBAL_TYPE(CharacterDetailsComponentDef));
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::DynamicLight)]) {
            entityDef->components.emplace_back(ComponentTypes::DynamicLight);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Inventory)]) {
            entityDef->components.emplace_back(ComponentTypes::Inventory);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Navigation)]) {
            entityDef->components.emplace_back(ComponentTypes::Navigation);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::PersonAI)]) {
            entityDef->components.emplace_back(ComponentTypes::PersonAI);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Physics)]) {
            //auto& physics = mTemplateRegistry.emplace<PhysicsComponent>(templateEntity/*, mWorld, position, false*/);
            //physics.mQueryActorTypes = ACTORTYPE_HUMAN;
            //physics.addCollider(newEntity, ColliderShapes::SPHERE, SPRITE_RADIUS);
            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::Physics);
            fileData.physics = PhysicsComponentDef(); // Default initialize
            keg::parse((ui8*)&fileData.physics, value, readContext, &KEG_GLOBAL_TYPE(PhysicsComponentDef));
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Profession)]) {
            entityDef->components.emplace_back(ComponentTypes::Profession);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::SimpleSprite)]) {
            entityDef->components.emplace_back(ComponentTypes::SimpleSprite);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::SoldierAI)]) {
            entityDef->components.emplace_back(ComponentTypes::SoldierAI);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::UndeadAI)]) {
            entityDef->components.emplace_back(ComponentTypes::UndeadAI);
        }
        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Skills)]) {
            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::Skills);
            keg::parse((ui8*)&fileData.skillsFileData, value, readContext, &KEG_GLOBAL_TYPE(SkillsComponentFileData));
        }
        else {
            pError("Tried to load invalid .entt component type \"" + key + "\"");
        }
        static_assert(e_cast(ComponentTypes::COUNT) == 15, "Parse new component type");
        // Load data
        //BuildingDescription description
    }))) {
        //mTemplateEntities[filePath.getFileNameNoExtension()] = templateEntity;
        nString fileName = filePath.getFileNameNoExtension();
        if (fileName.size() > MAX_CHARS_IN_STRTOKEN_WITH_INDEX) {
            pError("Entity file name " + fileName + " is too long. It must be 12 characters + 1 integer at the end, or less.");
        }
        else {
            mEntityDefinitions[StrToken(fileName)] = std::move(entityDef);
        }
    } else {
        // Failure case
        pError("Failed to parse entity file " + filePath.getString());
    }
}

const EntityDefinition& EntityDefinitionRepository::getDefinition(StrToken typeToken)
{
    auto&& it = mEntityDefinitions.find(typeToken);
    if (it == mEntityDefinitions.end()) {
        LOG_CRITICAL("Failed to find entity {}", typeToken.toString().c_str());
        LOG_CRITICAL("  Available entities:");
        for (auto&& it2 : mEntityDefinitions) {
            LOG_CRITICAL("  {}", it2.first.toString().c_str());
        }
        abort();
    }
    return *it->second;
}
