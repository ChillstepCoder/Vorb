#include "stdafx.h"

#include "ResourceOperations.h"

#include "resources/ModelRepository.h"

bool ResourceOperations::importFbxModel(const vio::Path& path) {
    return ModelRepository::get().loadFbxFile(path);
}