#pragma once

#include "component/ComponentTypes.h"
#include "util/StrToken.h"

DECL_VIO(class IOManager);

// TODO: Investigate optimizations such as groups, also look at serialization snapshots/archives
// https://skypjack.github.io/entt/md_docs_md_entity.html

struct EntityDefinition;
class IEntityComponentSystem;
class ResourceManager;
typedef std::unordered_map<StrToken, std::unique_ptr<EntityDefinition>> EntityDefinitionMap;

class EntityDefinitionRepository
{
public:
    EntityDefinitionRepository(vio::IOManager& ioManager);
    ~EntityDefinitionRepository();

    void loadEntityDefinitionFile(const vio::Path& filePath);
    const EntityDefinition& getDefinition(StrToken typeToken);

    const EntityDefinitionMap& getAllEntityDefinitions() const { return mEntityDefinitions; }

private:
    EntityDefinitionMap mEntityDefinitions;

    vio::IOManager& mIoManager;
};

