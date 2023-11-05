#pragma once

#include "rendering/model/MaterialRenderPassType.h"

class Mesh;


class DynamicModelInstanceData
{
public:
    std::vector<ui32> mVisibleIndices;
    AssetHandle<ModelDef> mModelHandle;
    mutable std::unique_ptr<GLDrawCommandBuffer> mDrawCommands;
    struct MeshData {
        const Mesh* mesh = nullptr;
        MeshLODDrawInfo drawInfos[4];
    };
    std::vector<MeshData> mMeshData;
    bool mIsInitialized = false;
};
