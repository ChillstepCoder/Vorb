#include "stdafx.h"
#include "ecs/EntityRepository.h"

#include "entt/entity/storage.hpp"

#include "ecs/IEntityComponentSystem.h"

#include "definitions/EntityDef.h"

#include "resources/ResourceManager.h"

#include <Vorb/io/IOManager.h>
//
//
//void EntityRepository::loadEntityDefinitionFile(const vio::Path& filePath)
//{
//    std::unique_ptr<EntityDef> entityDef = std::make_unique<EntityDef>();
//
//    if (mIoManager.parseFileAsKegObjectMap(filePath, makeFunctor([&](Sender s, const nString& key, keg::Node value) {
//        keg::ReadContext& readContext = *((keg::ReadContext*)s);
//
//        if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterModel)]) {
//            entityDef->components.emplace_back(ComponentTypes::CharacterModel);
//        } else if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterControl)]) {
//            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::CharacterControl);
//            fileData.characterControl = CharacterControlComponentDef(); // Default initialize
//            keg::parse((ui8*)&fileData.characterDetails, value, readContext, &KEG_GLOBAL_TYPE(CharacterControlComponentDef));
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Combat)]) {
//            entityDef->components.emplace_back(ComponentTypes::Combat);
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Corpse)]) {
//            entityDef->components.emplace_back(ComponentTypes::Corpse);
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterDetails)]) {
//            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::CharacterDetails);
//            fileData.characterDetails = CharacterDetailsComponentDef(); // Default initialize
//            keg::parse((ui8*)&fileData.characterDetails, value, readContext, &KEG_GLOBAL_TYPE(CharacterDetailsComponentDef));
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::DynamicLight)]) {
//            entityDef->components.emplace_back(ComponentTypes::DynamicLight);
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Inventory)]) {
//            entityDef->components.emplace_back(ComponentTypes::Inventory);
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Navigation)]) {
//            entityDef->components.emplace_back(ComponentTypes::Navigation);
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::PersonAI)]) {
//            entityDef->components.emplace_back(ComponentTypes::PersonAI);
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Physics)]) {
//            //auto& physics = mTemplateRegistry.emplace<PhysicsComponent>(templateEntity/*, mWorld, position, false*/);
//            //physics.mQueryActorTypes = ACTORTYPE_HUMAN;
//            //physics.addCollider(newEntity, ColliderShapes::SPHERE, SPRITE_RADIUS);
//            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::Physics);
//            fileData.physics = PhysicsComponentDef(); // Default initialize
//            keg::parse((ui8*)&fileData.physics, value, readContext, &KEG_GLOBAL_TYPE(PhysicsComponentDef));
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Profession)]) {
//            entityDef->components.emplace_back(ComponentTypes::Profession);
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::SimpleSprite)]) {
//            entityDef->components.emplace_back(ComponentTypes::SimpleSprite);
//        }
//        else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Skills)]) {
//            ComponentDefinition& fileData = entityDef->components.emplace_back(ComponentTypes::Skills);
//            keg::parse((ui8*)&fileData.skillsFileData, value, readContext, &KEG_GLOBAL_TYPE(SkillsComponentFileData));
//        }
//        else {
//            pError("Tried to load invalid .entt component type \"" + key + "\"");
//        }
//        static_assert(e_cast(ComponentTypes::COUNT) == 15, "Parse new component type");
//        // Load data
//        //BuildingDescription description
//    }))) {
//        //mTemplateEntities[filePath.getFileNameNoExtension()] = templateEntity;
//        nString fileName = filePath.getFileNameNoExtension();
//        if (fileName.size() > MAX_CHARS_IN_STRTOKEN) {
//            pError("Entity file name " + fileName + " is too long. It must be 12 characters + 1 integer at the end, or less.");
//        }
//        else {
//            mEntityDefinitions[StrToken(fileName)] = std::move(entityDef);
//        }
//    } else {
//        // Failure case
//        pError("Failed to parse entity file " + filePath.getString());
//    }
//}

AssetLoadFunc EntityRepository::getAssetLoadFunc() {
    return[&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        EntityDef& def = *static_cast<EntityDef*>(assetDataPtr);

        nString fileData = readFileToString(filePath);

        ryml::Tree tree = YmlSerializer::parseFileData(fileData);

        ryml::ConstNodeRef rootNode = tree.rootref();

        for (ryml::ConstNodeRef innerNode : rootNode.children()) {
            StrToken key(std::string_view(innerNode.key().str, innerNode.key().len));

            if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterModel)]) {
                CharacterModelComponentDef& cmpDef = def.components.emplace_back(ComponentTypes::CharacterModel).characterModel;
                innerNode >> cmpDef;
                if (cmpDef.model.isValid()) {
                    def.addDependency(cmpDef.model.getAssetHandleBase());
                }
                if (cmpDef.animMachine.isValid()) {
                    def.addDependency(cmpDef.animMachine.getAssetHandleBase());
                }
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterControl)]) {
                innerNode >> def.components.emplace_back(ComponentTypes::CharacterControl).characterControl;
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Combat)]) {
                def.components.emplace_back(ComponentTypes::Combat);
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Corpse)]) {
                def.components.emplace_back(ComponentTypes::Corpse);
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::CharacterDetails)]) {
                innerNode >> def.components.emplace_back(ComponentTypes::CharacterDetails).characterDetails;
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::DynamicLight)]) {
                def.components.emplace_back(ComponentTypes::DynamicLight);
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Inventory)]) {
                def.components.emplace_back(ComponentTypes::Inventory);
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Navigation)]) {
                def.components.emplace_back(ComponentTypes::Navigation);
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::PersonAI)]) {
                def.components.emplace_back(ComponentTypes::PersonAI);
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Physics)]) {
                innerNode >> def.components.emplace_back(ComponentTypes::Physics).physics;
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Profession)]) {
                def.components.emplace_back(ComponentTypes::Profession);
            }
            else if (key == ComponentTypeStrings[e_cast(ComponentTypes::Skills)]) {
                innerNode >> def.components.emplace_back(ComponentTypes::Skills).skillsFileData;
            }
            else {
                pError("Tried to load invalid .entt component type \"" + key.toString() + "\"");
            }
            static_assert(e_cast(ComponentTypes::COUNT) == 12, "Parse new component type");
        }
        // Wait for dependencies to load if needed
        if (def.getDependencies()->getCount()) {
            assetLoader.requestAssetLoadWithDependencies(
                nullptr,
                nullptr,
                assetID,
                assetDataPtr,
                filePath,
                mLoadedAssets[assetID].get(),
                nullptr,
                def.getDependencies()
            );
            return false;
        }
        else {
            return true;
        }
    };
}
