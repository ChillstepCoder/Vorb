#pragma once

#include "definitions/ModelDef.h"

DECL_VIO(class IOManager);

typedef ui32 ModelID;

class ModelRepository
{
public:
    ModelRepository(vio::IOManager& ioManager);
    ~ModelRepository();

    bool loadModelFile(const vio::Path& filePath);

private:
    vio::IOManager& mIoManager;
    std::vector<ModelDef> mModelDefs;
};

