#include "stdafx.h"
#include "OrthoCamera.h"

bool OrthoCamera::sphereIsVisible(const f32v3& pos, float radius) const
{
    throw std::logic_error("The method or operation is not implemented.");
}

f32v3 OrthoCamera::screenToWorld(f32v2 screenPos, f32 depth /*= 0.0f*/) const {
    f32v4 pos4 = f32v4(screenPos.x, screenPos.y, depth, 1.0f);
    pos4 = mMatrices.inverseP * pos4;
    return f32v3(pos4.x + mPosition.x * 2.0f, pos4.y + mPosition.y * 2.0f, pos4.z);
}

void OrthoCamera::updateProjection() {
    f32v3 halfDims = mDims * 0.5f / mZoom;
    mMatrices.P = glm::ortho(
        -halfDims.x, halfDims.x,
        -halfDims.y, halfDims.y,
        -halfDims.z, halfDims.z
    );
    mMatrices.inverseP = glm::inverse(mMatrices.P);
}
