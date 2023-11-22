#pragma once
#include "camera/ICamera.h"

class OrthoCamera : public ICamera
{
public:

	f32 getZAngle() const override { return 0.0f; }
	f32 getZNear() const override { return 0.0f; }
	f32 getZFar() const override { return FLT_MAX; };

	bool sphereIsVisible(const f32v3& pos, float radius) const override;
	// Center of screen is here
	f32v3 getOrthoPosition() const { return mOrthoPosition; }
	void setOrthoPosition(f32v3 pos) { mOrthoPosition = pos; }
    f32v3 getDims() const { return mDims; }
    void setDims(f32v3 dims) { mDims = dims; }
	f32 getZoom() const { return mZoom; }
	void setZoom(f32 zoom) { mZoom = zoom; }
protected:
	void updateProjection() override;

	f32v3 mOrthoPosition = f32v3(0.0);
	// [-1, 1] default
	f32v3 mDims = f32v3(2.0, 2.0, 600000.0);
	f32 mZoom = 0.0f; // 0.0f - 1.0f
};

