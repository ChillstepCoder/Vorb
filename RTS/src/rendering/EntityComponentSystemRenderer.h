#pragma once

#include <Vorb/graphics/Texture.h>

DECL_VG(class SpriteBatch)
DECL_VG(class DepthState)

class World;
class Camera3D;
class LightRenderer;

class EntityComponentSystemRenderer {
public:
	EntityComponentSystemRenderer();
    ~EntityComponentSystemRenderer();
    void renderBusinessDebug(World& world, const Camera3D& camera) const;
	void renderDynamicLightComponents(World& world, const Camera3D& camera, const LightRenderer& lightRenderer);
	void renderInteractUI(const Camera3D& camera) const;

private:
	std::unique_ptr<vg::SpriteBatch> mSpriteBatch;

    mutable int mFrameCount = 0;
    const int mFramesPerDebugDraw = 12;

};

