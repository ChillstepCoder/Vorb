#pragma once

#include "rendering/mesh/VertexType.h"
#include "rendering/mesh/MeshLODData.h"
#include "rendering/mesh/MeshIndexType.h"
#include "rendering/model/MaterialRenderPassType.h"

using ModelBatchID = ui8;
constexpr ModelBatchID INVALID_MODEL_BATCH_ID = std::numeric_limits<ModelBatchID>::max();

class ModelBatch {
    friend class ModelRepository; // Managed by
public:
    ModelBatch() = default;
    ~ModelBatch();
    VORB_NON_COPYABLE(ModelBatch);

    ui32 getTotalSizeBytes() const { return mVerticesSizeBytes + mIndicesSizeBytes; }

private:
    VGBuffer  mVao = 0;
    VGBuffer  mVbo = 0;
    VGBuffer  mIbo = 0;
    VertexType mVertexType = VertexType::INVALID;
    MeshIndexType mIndexType = MeshIndexType::INVALID;
    MaterialRenderPassType mRenderPass = MaterialRenderPassType::Default;
    ModelBatchID mBatchID = 0;
    ui32 mVerticesSizeBytes = 0; // Stores up to 4gb. If we have more than this we have a HUGE problem
    ui32 mIndicesSizeBytes = 0; // Stores up to 4gb. If we have more than this we have a HUGE problem
};


struct ModelBatchSubmeshDrawData {
    MaterialRenderPassType renderPass;
    ModelBatchID batchID;
    MeshLODData LODData;
};

using ModelBatchSubmeshDrawDataID = ui16;
constexpr ModelBatchSubmeshDrawDataID INVALID_MODEL_BATCH_SUBMESH_DRAW_DATA_ID = std::numeric_limits<ModelBatchSubmeshDrawDataID>::max();
constexpr int MAX_TOTAL_SUBMESHES = INVALID_MODEL_BATCH_SUBMESH_DRAW_DATA_ID - 1;
