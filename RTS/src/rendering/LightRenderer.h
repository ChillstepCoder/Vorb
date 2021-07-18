#pragma once
#include "LightData.h"

class ResourceManager;
class MaterialRenderer;
class Material;
class Camera2D;


// TODO: IRendererBase?
// TODO: Use point sprite for lights?
class LightRenderer
{
public:
    LightRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer);
    ~LightRenderer();

    void RenderLight(const f32v2& position, const LightData& lightData, const Camera2D& camera) const;

    void InitPostLoad();

private:
    void InitSharedMesh();

    ResourceManager& mResourceManager;
    const MaterialRenderer& mMaterialRenderer;
    const Material* mPointLightMaterial = nullptr;

    // TODO: UBO per light?

    struct PointLightMeshData {
        union {
            struct {
                ui32 mVb; ///< Vertex buffer ID
                ui32 mIb; ///< Index buffer ID
            };
            ui32 mBuffers[2]; ///< Storage for both buffers used by this mesh
        };
        ui32 mVao = 0; ///< VAO with vertex attribute pointing to 0
    } mSharedPointLightMesh;
};

