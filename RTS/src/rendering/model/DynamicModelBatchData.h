#pragma once

#include "rendering/model/MaterialRenderPassType.h"
#include "rendering/mesh/MeshConst.h"
#include "definitions/ModelDef.h"

class Mesh;

class DynamicModelBatchData
{
public:
    AssetHandlePtr<ModelDef> mModelHandle;
    struct MeshData {
        const Mesh* mesh = nullptr;
        MeshLODDrawInfo drawInfos[4];
        mutable std::unique_ptr<GLDrawCommandBuffer> drawCommands;
        int visibleCount = 0;
    };
    std::vector<ui32> mVisibleIndices;
    std::vector<MeshData> mMeshData;
    bool mIsInitialized = false;
    bool mDidRegisterCommands = false;
    f32 mBoundingSphereRadius = 5.0f;
};
