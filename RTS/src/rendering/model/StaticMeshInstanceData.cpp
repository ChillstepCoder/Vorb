#include "stdafx.h"
#include "StaticMeshInstanceData.h"

StaticMeshInstanceData::StaticMeshInstanceData()/* : mNumVisibleMeshesBuffer(sizeof(ui32), nullptr, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)*/
{
    // TODO: Investigate why, hardware? Driver? - Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
   /* mNumVisibleMeshesBufferPtr = (uint32_t*)glMapNamedBuffer(mNumVisibleMeshesBuffer.getHandle(), GL_READ_WRITE);
    assert(mNumVisibleMeshesBufferPtr);*/

}