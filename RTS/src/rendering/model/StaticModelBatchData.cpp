#include "stdafx.h"
#include "StaticModelBatchData.h"

StaticModelBatchData::StaticModelBatchData()/* : mNumVisibleMeshesBuffer(sizeof(ui32), nullptr, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)*/ {
    mModelDamageZonesGpuData.emplace_back();
}