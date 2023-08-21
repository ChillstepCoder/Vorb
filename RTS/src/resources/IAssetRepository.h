#pragma once

#include "resources/IAsset.h"

DECL_VIO(class IOManager);

class IAssetRepository
{
public:
    IAssetRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {}
    virtual ~IAssetRepository() = default;

protected:
    vio::IOManager& mIoManager;
    bool saveAssetContents(const IAsset& asset, const char* fileContents, size_t sizeBytes);
};

