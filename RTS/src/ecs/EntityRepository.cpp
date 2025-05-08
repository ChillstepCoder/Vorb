#include "stdafx.h"
#include "ecs/EntityRepository.h"

#include "entt/entity/storage.hpp"

#include "ecs/IFullECS.h"

#include "definitions/EntityDef.h"

#include "resources/ResourceManager.h"

#include <Vorb/io/IOManager.h>

AssetLoadFunc EntityRepository::getAssetLoadFunc() {
    return[&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        EntityDef& def = *static_cast<EntityDef*>(assetDataPtr);

        nString fileData = readFileToString(filePath);

        ryml::Tree tree = YmlSerializer::parseFileData(fileData);

        ryml::ConstNodeRef rootNode = tree.rootref();

        for (ryml::ConstNodeRef innerNode : rootNode.children()) {
            const std::string_view strv(innerNode.key().str, innerNode.key().len);
            // Ignore null node (happens sometimes)
            if (!strv.size() || strv[0] == '\0') {
                continue;
            }
            const StrToken key(strv);

            // Simple linear search, small elements so should be very fast
            ComponentType cmpType = ComponentType::COUNT;
            for (const ComponentType e : enum_range(ComponentType::BEGIN, ComponentType::END)) {
                if (key == ComponentTypeStrings[e_cast(e)]) {
                    cmpType = e;
                    break;
                }
            }
            if (cmpType == ComponentType::COUNT) [[unlikely]] {
                panic("Invalid component type {} in {}", key.toString(), filePath.getCString());
            }

            ComponentDefinitionInstance& newCmpInst = def.components.emplace_back(cmpType);

            // Helper
#define ALLOCATE_AND_PARSE_DEF(type, varName) \
            newCmpInst.componentDef = std::make_unique<type>(); \
            type& varName = static_cast<type&>(*newCmpInst.componentDef); \
            innerNode >> varName;

            // Parse data and allocate component def if needed
            switch (cmpType) {
                case ComponentType::CharacterModel: {
                    ALLOCATE_AND_PARSE_DEF(CharacterModelComponentDef, cmpDef);
                    if (cmpDef.model.isValid()) {
                        def.addDependency(cmpDef.model.getAssetHandleBase());
                    }
                    break;
                }
                case ComponentType::CharacterControl: {
                    ALLOCATE_AND_PARSE_DEF(CharacterControlComponentDef, cmpDef);
                    break;
                }
                case ComponentType::Combat:
                    break;
                case ComponentType::Corpse:
                    break;
                case ComponentType::CharacterDetails: {
                    ALLOCATE_AND_PARSE_DEF(CharacterDetailsComponentDef, cmpDef);
                    break;
                }
                case ComponentType::DynamicLight:
                    break;
                case ComponentType::Inventory:
                    break;
                case ComponentType::Navigation:
                    break;
                case ComponentType::PersonAI:
                    break;
                case ComponentType::Physics: {
                    ALLOCATE_AND_PARSE_DEF(PhysicsComponentDef, cmpDef);
                    break;
                }
                case ComponentType::Profession:
                    break;
                case ComponentType::Skills: {
                    ALLOCATE_AND_PARSE_DEF(SkillsComponentDef, cmpDef);
                    break;
                }
                default:
                    panic("Unhandled component type {}", (int)cmpType);
                    break;

            }
            static_assert(e_cast(ComponentType::COUNT) == 12, "Parse new component type");
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
