#include "stdafx.h"
#include "IAssetRepository.h"

#include <Vorb/io/IOManager.h>

bool IAssetRepository::saveAssetContents(const IAsset& asset, const char* fileContents, size_t sizeBytes) {
    assert(!asset.getDiskLocation().isNull());

    assert(fileContents);

    vio::Path absolutePath;

    if (!mIoManager.assurePath(asset.getDiskLocation(), absolutePath, vio::IOManagerDirectory::CURRENT_WORKING, true)) {
        LOG_CRITICAL("Failed to evaluate file path in IAssetRepository::saveAssetContents: {}", asset.getDiskLocation().getString());
    }

    const nString& filePath = absolutePath.getString();
    std::ofstream file(filePath, std::ios::out);

    if (!file.is_open()) {
        LOG_CRITICAL("Failed to open file for writing in IAssetRepository::saveAssetContents: {}", filePath);
        return false;
    }

    // Write the content to the file
    file.write(fileContents, sizeBytes);

    // Check for any errors during the write operation
    if (file.fail()) {
        LOG_CRITICAL("Error occurred while writing to the file: {} code: {}", filePath, file.rdstate());
        return false;
    }

    asset.setDirty(false);
}
