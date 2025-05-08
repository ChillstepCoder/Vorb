#pragma once

#include "resources/IAssetRepository.h"
#include "component/ComponentType.h"

#include "definitions/EntityDef.h"

DECL_VIO(class IOManager);

// TODO: Investigate optimizations such as groups, also look at serialization snapshots/archives
// https://skypjack.github.io/entt/md_docs_md_entity.html

class EntityDef;
class IFullECS;
class ResourceManager;

class EntityRepository : public IAssetRepository<EntityDef>
{
public:
    ASSET_REPOSITORY_COMMON_CODE(EntityRepository, EntityDef, AssetType::Entity)

    bool saveAsset(AssetID assetId) override { panic("Cannot save entities yet"); }

    StrToken getAssetExtension() const override { return CStrToken("ent"); }
    const char* const getAssetTypeDisplayName() const override { return "Entity"; }

private:
    AssetLoadFunc getAssetLoadFunc() override;
};

