#include "stdafx.h"
#include "ModelDef.h"

f32 ModelDef::getRandomScaleAtPosition(f32v2 position) const {

    if (mScaleRange.x == mScaleRange.y) {
        return mScaleRange.x;
    }
    const f32 diff = mScaleRange.y - mScaleRange.x;
    return mScaleRange.x + diff * Random::getThreadSafef(position.y, position.x);
}

f32 ModelDef::getScaleFromFloraAge(ui8 age) const {
    const f32 diff = mScaleRange.y - mScaleRange.x;
    return mScaleRange.x + diff * ((f32)age / 255.f);
}

i32 ModelDef::getRandomSubmeshIndex(f32 randomWeight) const {
    for (size_t i = 0; i < mRandomPolyCpuDataWeights.size(); i++) {
        if (randomWeight <= mRandomPolyCpuDataWeights[i]) {
            return (i32)i;
        }
    }
    return (i32)mRandomPolyCpuDataWeights.size() - 1;
}
