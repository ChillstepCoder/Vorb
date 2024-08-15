#pragma once

#include "rendering/mesh/VertexType.h"
#include "rendering/mesh/MeshLODData.h"
#include "rendering/mesh/MeshIndexType.h"
#include "rendering/model/MaterialRenderPassType.h"

class ModelBatch {
    friend class ModelRepository; // Managed by
public:
    ModelBatch() = default;
    ~ModelBatch();
    VORB_NON_COPYABLE(ModelBatch);

private:
    VGBuffer  mVao = 0;
    VGBuffer  mVbo = 0;
    VGBuffer  mIbo = 0;
    VertexType mVertexType = VertexType::INVALID;
    MeshIndexType mIndexType = MeshIndexType::INVALID;
    MaterialRenderPassType mRenderPass = MaterialRenderPassType::Default;
    ui16 mBatchID = 0;
};

class ModelBatchSubmesh {
    ui16 mBatchID;
    MeshLODData mLODData;
};

// Models are batched based on their render pass, index type, and vertex type
struct ModelBatchKey {
    VertexType mVertexType;
    MeshIndexType mIndexType;
    MaterialRenderPassType mRenderPass;

    auto operator<=>(const ModelBatchKey&) const = default;
};

using ModelBatchID = ui16;
constexpr ModelBatchID INVALID_MODEL_BATCH_ID = std::numeric_limits<ModelBatchID>::max();