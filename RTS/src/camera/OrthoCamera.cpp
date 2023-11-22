#include "stdafx.h"
#include "OrthoCamera.h"

bool OrthoCamera::sphereIsVisible(const f32v3& pos, float radius) const
{
    throw std::logic_error("The method or operation is not implemented.");
}

void OrthoCamera::updateProjection() {
    const f32v3 zoomAdjust = mDims * mZoom;
    mMatrices.P = glm::ortho(
        0.0f - zoomAdjust.x, 0.0f + zoomAdjust.x,
        0.0f - zoomAdjust.y, 0.0f + zoomAdjust.y,
        0.0f - zoomAdjust.z, 0.0f + zoomAdjust.z
    );
    mMatrices.inverseP = glm::inverse(mMatrices.P);
}
