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
	void setXYPos(f32v2 pos) { mPosition.x = pos.x; mPosition.y = pos.y; }
    f32v3 getDims() const { return mDims; }
	void setXYDims(f32v2 dims) { mDims.x = dims.x; mDims.y = dims.y; mDirtyProjection = true; }
    void setDims(f32v3 dims) { mDims = dims; mDirtyProjection = true;  }
	f32 getZoom() const { return mZoom; }
	void setZoom(f32 zoom) { mZoom = zoom; mDirtyProjection = true;}

	f32v3 screenToWorld(f32v2 screenPos, f32 depth = 0.0f) const;
protected:
	void updateProjection() override;
	// [-1, 1] default
	f32v3 mDims = f32v3(2.0, 2.0, 600000.0);
	f32 mZoom = 0.0f; // 0.0f - 1.0f
};

