#pragma once

#include "rendering/mesh/MeshConst.h"

struct MeshLODData {

    MeshLODData& operator=(const MeshLODData& o) {
        this->mTotalIndexCount = o.mTotalIndexCount;
        memcpy(this->mLODStarts, o.mLODStarts, sizeof(ui32) * e_cast(MeshLODLevel::COUNT));
        return *this;
    }

    // TODO: High start is always 0 so why store it
    ui32 mLODStarts[e_cast(MeshLODLevel::COUNT)] = {}; // Index buffer start for each LOD level
    ui32 mTotalIndexCount = 0;

    ui32 getHighestLODIndexCount() const {
        return mLODStarts[1];
    }

    MeshLODDrawInfo getDrawInfoForLOD(MeshLODLevel lod) const {
        assert(lod != MeshLODLevel::COUNT && "Invalid LOD level");
        const ui32 start = mLODStarts[e_cast(lod)];
        // TODO: Remove branching?
        if (lod == MeshLODLevel::Lowest) {
            return MeshLODDrawInfo{ start, mTotalIndexCount - start };
        }
        return MeshLODDrawInfo{ start, mLODStarts[e_cast(lod) + 1] - start };
    }

    BINARY_SERIALIZE() {
        for (int i = 0; i < e_count(MeshLODLevel); ++i) {
            s.value4b(mLODStarts[i]);
        }
        s.value4b(mTotalIndexCount);
    }
};

