#include "stdafx.h"
#include "TextureManip.h"

#include "rendering/TextureAtlas.h"
#include "ResourceManager.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"
#include <Vorb/graphics/SamplerState.h>

GPUTextureManipulator::GPUTextureManipulator(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer)
{
    mQuadVbo.init();
}

// Generates baked normal maps for every texture in the texture atlas
void GPUTextureManipulator::GenerateNormalMapsForTextureAtlas()
{
    PreciseTimer timer;

    glTextureBarrier();
    GLuint mFramebufferID = 0;
    glGenFramebuffers(1, &mFramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    glViewport(0, 0, TEXTURE_ATLAS_WIDTH_PX, TEXTURE_ATLAS_WIDTH_PX);

    const TextureAtlas& atlas = mResourceManager.getTextureAtlas();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlas.getAtlasTexture());

    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    // Gather all sprites to generate maps for
    std::map<int /*page*/, std::vector<SpriteData*>> spritesToGenerate;
    for (auto&& it : mResourceManager.getSpriteRepository().getSprites()) {
        SpriteData& spriteData = it.second;
        if (spriteData.flags & SPRITEDATA_FLAG_HAS_NORMAL_MAP) {
            continue;
        }
        spritesToGenerate[spriteData.atlasPage].push_back(&spriteData);
        spriteData.flags |= SPRITEDATA_FLAG_HAS_NORMAL_MAP;
    }

    VGUniform uvUniform = mNormalsMaterial->mProgram.getUniform("unUvRect");
    VGUniform pixelDimsUniform = mNormalsMaterial->mProgram.getUniform("unPixelDims");
    VGUniform atlasUniform = mNormalsMaterial->mProgram.getUniform("unAtlasPage");

    // Generate temporary texture
    VGTexture tmpPageTexture;
    glGenTextures(1, &tmpPageTexture);
    glBindTexture(GL_TEXTURE_2D, tmpPageTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_ATLAS_WIDTH_PX, TEXTURE_ATLAS_WIDTH_PX, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    // Set up tex parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (int)0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, (int)0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    for (auto&& it : spritesToGenerate) {
        assert(it.first < (int)atlas.getNumPages());
        // Set read buffer for copy
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, atlas.getAtlasTexture(), 0, it.first);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, TEXTURE_ATLAS_WIDTH_PX, TEXTURE_ATLAS_WIDTH_PX, 0);
        glReadBuffer(0);
        // Select normals page
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, atlas.getAtlasTexture(), 0, it.first + 1);

        ui32 nextAvailableTextureIndex;
        mMaterialRenderer.bindMaterialForRender(*mNormalsMaterial, &nextAvailableTextureIndex);
        glActiveTexture(GL_TEXTURE0 + nextAvailableTextureIndex);
        glUniform1i(atlasUniform, nextAvailableTextureIndex);
        glUniform2f(pixelDimsUniform, 1.0f / TEXTURE_ATLAS_WIDTH_PX, 1.0f / TEXTURE_ATLAS_WIDTH_PX);
        

        // Loop through each sprite on this layer
        for (SpriteData* sprite : it.second) {

            // Method specific generation
            switch (sprite->method)
            {
                case TileTextureMethod::SIMPLE:
                case TileTextureMethod::FLORA: {
                    glUniform4f(uvUniform, sprite->uvs.x, sprite->uvs.y, sprite->uvs.z, sprite->uvs.w);
                    mQuadVbo.draw();
                    break;
                }
                case TileTextureMethod::CONNECTED_WALL: {
                    for (int y = 0; y < TILE_TEX_METHOD_CONNECTED_WALL_HEIGHT; ++y) {
                        for (int x = 0; x < TILE_TEX_METHOD_CONNECTED_WALL_WIDTH; ++x) {
                            glUniform4f(
                                uvUniform,
                                sprite->uvs.x + x * sprite->uvs.z,
                                sprite->uvs.y + y * sprite->uvs.w,
                                sprite->uvs.z,
                                sprite->uvs.w
                            );
                            mQuadVbo.draw();
                        }
                    }
                    break;
                }
                default:
                    assert(0); // Implement missing type
                    break;
            }
        }
        static_assert((int)TileTextureMethod::COUNT == 4, "Implement normal generation for new method");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glTextureBarrier();
    glDeleteTextures(1, &tmpPageTexture);
    printf("Generated normal maps in %.2lf ms\n", timer.stop());
    checkGlError("Generate Normal Maps End");
}

void GPUTextureManipulator::InitPostLoad() {
    mNormalsMaterial = mResourceManager.getMaterialManager().getMaterial("normals_gen");
    GenerateNormalMapsForTextureAtlas();

    // Generate mipmaps
    const TextureAtlas& atlas = mResourceManager.getTextureAtlas();
    atlas.generateMipMaps();
}
