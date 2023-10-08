#include "stdafx.h"
#include "IAssetRepository.h"

#include <Vorb/io/IOManager.h>

bool IAssetRepositoryBase::saveAssetContents(const IAsset& asset, const vio::Path& path, const char* fileContents, size_t sizeBytes) {
    assert(!path.isNull());

    assert(fileContents);

    vio::Path absolutePath;
    if (path.isAbsolute()) {
        absolutePath = path;
    }
    else {
        if (!mIoManager.assurePath(path, absolutePath, vio::IOManagerDirectory::CURRENT_WORKING, true)) {
            LOG_CRITICAL("Failed to evaluate file path in IAssetRepository::saveAssetContents: {}", path.getString());
        }
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
    return true;
}

nString IAssetRepositoryBase::readFileToString(const vio::Path& path) {
    nString fileData;
    if (!mIoManager.readFileToString(path, fileData)) {
        panic("Asset repository failed to read file {}", path.getCString());
    }
    return fileData;
}

AssetHandleBasePtr IAssetRepositoryBase::getAssetHandleBase(StrToken assetName) {
    return getAssetHandleBase(mAssetLookup.at(assetName));
}

AssetHandleBasePtr IAssetRepositoryBase::getAssetHandleBase(AssetID id) {
    AssetHandleBasePtr handle = makeAssetHandle();
    aquireAssetHandle(id, *handle);
    return handle;
}
