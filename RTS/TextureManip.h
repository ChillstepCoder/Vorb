#pragma once


// THE IDEAL: Each Framebuffer is aware of if it is being written or read from, and the entire chain
// so we can error check detect cycles.
// New framebuffer objects are created as needed?

// Describe a material pipeline in data


#include <Vorb/graphics/Texture.h>
#include <Vorb/graphics/GLRenderTarget.h>

#include "rendering/Material.h"
#include <Vorb/graphics/FullQuadVBO.h>

class TextureAtlas;
class ResourceManager;
class MaterialRenderer;
class Material;

//#include <Vorb/graphics/RTSwapChain.hpp>

enum class TextureBlendMode {
    REPLACE,
    ALPHA,
    ADD,
    MULTIPLY,
    //  SOFT_LIGHT? :P
};

// Data describing a simplex noise function
//struct NoiseParameters {
//
//};

// Fire example?
// INPUT: Depth buffer
// INPUT: Color buffer
// Writes to depth when color is high enough? :thinking:
// Output: New Color Buffer

enum class GPUTextureProcess {
    GENERATE_NORMALS
};

//struct Mesh {
//    VGVertexArray mVao = 0; ///< Vertex Array Object
//    VGBuffer mVbo = 0; ///< Vertex Buffer Object
//    VGBuffer mIbo = 0; ///< Index Buffer Object
//    VGTexture mTexture = 0;
//    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
//};

// Maniuplate textures on the GPU
class GPUTextureManipulator {
public:
    GPUTextureManipulator(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer);
    void GenerateNormalMapsForTextureAtlas();

    void InitPostLoad();

    // Take a source material and run it through a material filter, applying a shader and giving a result texture
    //void RunMaterialProcess(vg::Texture source, GPUTextureProcess process, std::function<int(vg::Texture)> onFinish);
    // Fire particles? Write to screen instead of texture
    //void RenderMaterialToVBO();

    //struct Task {
    //    vg::Texture source;
    //    GPUTextureProcess process;
    //    std::function<int(vg::Texture)> onFinish;
    //};

    //void update();

    ////vg::RTSwapChain<2> swapChain;

    //vg::GLRenderTarget mFBOPool;
    //std::vector<Task> mTasks;
private:
    ResourceManager& mResourceManager;
    const MaterialRenderer& mMaterialRenderer;
    const Material* mNormalsMaterial = nullptr;
    vg::FullQuadVBO mQuadVbo;
};

class Camera2D;

// TODO: Move to Data?
//class FireMaterial : public Material {
//
//    TextureHandle inputDepth;
//};
//
//class Material {
//
//    // Interesting question, can passing vec3 by reference reduce performance?
//    void DrawWorldSpace(Camera2D& camera, const f32v2& position, const f32v2& dims);
//    void DrawScreenSpace(Camera2D& camera, const f32v2& position, const f32v2& dims);
//
//    TextureHandle texture1;
//    TextureHandle normalMap;
//};
//
//class TextureHandle : public vg::Texture {
//public:
//    TextureHandle();
//    TextureHandle(vg::Texture&& vgTexture);
//
//    enum class CacheMode {
//        NONE,
//        RAM,
//        DISK
//    };
//
//    void SetCacheMode(TextureHandle::CacheMode mode);
//    TextureHandle::CacheMode GetCacheMode();
//
//    bool BlitTo(TextureHandle& target);
//    bool RunShader(TextureHandle& target);
//
//    bool DrawToScreen(Camera2D& camera, f32v2& position, f32v2& dimsScreen);
//private:
//    color4* pixelCache = nullptr;
//};
