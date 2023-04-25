#pragma once

#include <Vorb/graphics/Texture.h>

DECL_VG(class SpriteBatch)
DECL_VG(class DepthState)

class Camera3D;
class LightRenderer;

class EntityComponentSystemRenderer {
public:
	EntityComponentSystemRenderer();
    void renderBusinessDebug(const Camera3D& camera) const;
	void renderDynamicLightComponents(const Camera3D& camera, const LightRenderer& lightRenderer);
	void renderInteractUI(const Camera3D& camera) const;

private:
	std::unique_ptr<vg::SpriteBatch> mSpriteBatch;
    vg::Texture mCircleTexture;
    vg::Texture mSquareTexture;

    mutable int mFrameCount = 0;
    const int mFramesPerDebugDraw = 12;

};

