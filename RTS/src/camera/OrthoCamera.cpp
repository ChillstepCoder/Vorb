#include "stdafx.h"
#include "OrthoCamera.h"

bool OrthoCamera::sphereIsVisible(const f32v3& pos, float radius) const
{
    throw std::logic_error("The method or operation is not implemented.");
}

void OrthoCamera::updateProjection() {
    const f32v3 zoomAdjust = mDims * (1.0f - mZoom);
    mMatrices.P = glm::ortho(
        mOrthoPosition.x - zoomAdjust.x, mOrthoPosition.x + zoomAdjust.x,
        mOrthoPosition.y - zoomAdjust.y, mOrthoPosition.y + zoomAdjust.y,
        mOrthoPosition.z - zoomAdjust.z, mOrthoPosition.z + zoomAdjust.z
    );
    mMatrices.inverseP = glm::inverse(mMatrices.P);
}