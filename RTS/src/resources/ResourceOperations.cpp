#include "stdafx.h"

#include "ResourceOperations.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"

bool ResourceOperations::importFbxModel(const vio::Path& path) {
    ASSERT_RENDER_THREAD(); // This isnt actually a requirement but just need to make sure it handles other threads

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    ModelRepository& modelRepo = resourceManager.getModelRepository();
    return modelRepo.loadFbxFile(path, resourceManager.getMaterialRepository(), resourceManager.getAnimMachineRepository());
}