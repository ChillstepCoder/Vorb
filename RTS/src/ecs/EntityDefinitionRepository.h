#pragma once

#include "component/ComponentTypes.h"

DECL_VIO(class IOManager);

// TODO: Investigate optimizations such as groups, also look at serialization snapshots/archives
// https://skypjack.github.io/entt/md_docs_md_entity.html

struct EntityDefinition;
class EntityComponentSystem;
class ResourceManager;

class EntityDefinitionRepository
{
public:
    EntityDefinitionRepository(vio::IOManager& ioManager);
    ~EntityDefinitionRepository();

    void loadEntityDefinitionFile(const vio::Path& filePath);
    const EntityDefinition& getDefinition(const nString& typeName);

private:
    std::unordered_map<nString, std::unique_ptr<EntityDefinition>> mEntityDefinitions;

    vio::IOManager& mIoManager;
};

