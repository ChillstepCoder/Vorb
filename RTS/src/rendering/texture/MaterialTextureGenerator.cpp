#include "stdafx.h"
#include "MaterialTextureGenerator.h"

#include "util/TextureUtil.h"

#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/FullQuadVBO.h>
#include <Vorb/graphics/GLProgram.h>

const char* NORMAL_VERT_SRC = R"(
in vec2 vPosition; // Position in screen space
uniform vec4 unUvRect;
out vec2 fUV;
void main() {
    vec2 subUV = (vPosition + 1.0) / 2.0;
    fUV = unUvRect.xy + unUvRect.zw * subUV;
    gl_Position = vec4(fUV * 2.0 - 1.0, 0, 1);
}
)";

const char* NORMAL_FRAG_SRC = R"(
uniform sampler2D unTexture;
uniform vec2 unPixelDims;
uniform vec4 unUvRect;
in vec2 fUV;
out vec4 fColor;
const float THRESH = 0.01;
const vec3 COLOR = vec3(0.0, 0.0, 0.0);
const float INTENSITY = 60.0;

float luminance(vec3 pixel) {
    return 0.3 * pixel.r + 0.59 * pixel.g + 0.11 * pixel.b;
}

vec2 clampUV(vec2 UV) {
    float x = clamp(UV.x, unUvRect.x + unPixelDims.x * 0.5, unUvRect.x + unUvRect.z - unPixelDims.x * 0.5);
    float y = clamp(UV.y, unUvRect.y + unPixelDims.y * 0.5, unUvRect.y + unUvRect.w - unPixelDims.y * 0.5);
    return vec2(x, y);
}

vec3 getNormalCross() {
    // Get UVS
    vec2 leftUV = clampUV(vec2(fUV.x - unPixelDims.x, fUV.y));
    vec2 rightUV = clampUV(vec2(fUV.x - unPixelDims.x, fUV.y));
    vec2 topUV = clampUV(vec2(fUV.x, fUV.y - unPixelDims.y));
    vec2 bottomUV = clampUV(vec2(fUV.x, fUV.y + unPixelDims.y));

    // Sample neighbors
    vec3 left = texture(unTexture, leftUV).rgb;
    vec3 right = texture(unTexture, rightUV).rgb;
    vec3 top = texture(unTexture, topUV).rgb;
    vec3 bottom = texture(unTexture, bottomUV).rgb;

    // Compute luminances
    float leftL = luminance(left);
    float rightL = luminance(right);
    float topL = luminance(top);
    float bottomL = luminance(bottom);

    // Luminance offsets
    float dX = rightL - leftL;
    float dY = topL - bottomL;

    // Cross product
    vec3 rightVec = normalize(vec3(-unPixelDims.x * INTENSITY, 0.0, dX));
    vec3 frontVec = normalize(vec3(0.0, unPixelDims.y * INTENSITY, dY));
    return (normalize(cross(frontVec, rightVec)) + 1.0) / 2.0;
}

vec3 getNormalSobel() {
    // Get UVS
    vec2 leftUV = clampUV(vec2(fUV.x - unPixelDims.x, fUV.y));
    vec2 rightUV = clampUV(vec2(fUV.x - unPixelDims.x, fUV.y));
    vec2 topUV = clampUV(vec2(fUV.x, fUV.y - unPixelDims.y));
    vec2 bottomUV = clampUV(vec2(fUV.x, fUV.y + unPixelDims.y));

    // Sample neighbors
    vec3 left = texture(unTexture, leftUV).rgb;
    vec3 right = texture(unTexture, rightUV).rgb;
    vec3 top = texture(unTexture, topUV).rgb;
    vec3 bottom = texture(unTexture, bottomUV).rgb;

    // Compute luminances
    float leftL = luminance(left);
    float rightL = luminance(right);
    float topL = luminance(top);
    float bottomL = luminance(bottom);

    // Luminance offsets
    float dX = leftL - rightL;
    float dY = topL - bottomL;

    float xval = ((dX + 1.0) * 0.5);
    float yval = ((dY + 1.0) * 0.5);
    return vec3(xval, yval, 1.0);
}

void main() {
    fColor.rgb = getNormalSobel();
    fColor.a = 1.0;
}
)";

const char* STENCIL_VERT_SRC = R"(
in vec2 vPosition; // Position in screen space
uniform vec4 unUvRect;
out vec2 fUV;
void main() {
    vec2 subUV = (vPosition + 1.0) / 2.0;
    fUV = unUvRect.xy + unUvRect.zw * subUV;
    gl_Position = vec4(fUV * 2.0 - 1.0, 0, 1);
}
)";

const char* STENCIL_FRAG_SRC = R"(
uniform sampler2D unDiffuse;
uniform sampler2D unStencil;
uniform vec2 unPixelDims;
uniform vec4 unUvRect;
in vec2 fUV;
out vec4 fColor;

void main() {
    float stencil = texture(unStencil, fUV).r;
    fColor.rgb = texture(unDiffuse, fUV).rgb;
    fColor.a = stencil;
}
)";

MaterialTextureGenerator::MaterialTextureGenerator() {

}

MaterialTextureGenerator::~MaterialTextureGenerator() {

}

void onError(const nString& n) {
    pError("Failed to load internal normal map generator shader with error " + n);
}

void MaterialTextureGenerator::init() {
    glGenFramebuffers(1, &mFramebufferID);
    mNormalProgram = std::make_unique<vg::GLProgram>();
    eventpp::ScopedRemover<GLProgramErrorCallbackList> remover1(mNormalProgram->onShaderCompilationError);
    eventpp::ScopedRemover<GLProgramErrorCallbackList> remover2(mNormalProgram->onShaderCompilationError);
    remover1.append([](const nString& s) { onError(s); });
    remover2.append([](const nString& s) { onError(s); });
    // Normals
    mNormalProgram->init();
    mNormalProgram->addShader(vg::ShaderType::VERTEX_SHADER, NORMAL_VERT_SRC);
    mNormalProgram->addShader(vg::ShaderType::FRAGMENT_SHADER, NORMAL_FRAG_SRC);
    mNormalProgram->link();
    mNormalProgram->initUniforms();

    mUvRectUniform = mNormalProgram->getUniform("unUvRect");
    mTextureUniform = mNormalProgram->getUniform("unTexture");
    mPixelDimsUniform = mNormalProgram->getUniform("unPixelDims");

    checkGlError("NormalMapGenerator::init");
}

VGTexture MaterialTextureGenerator::generateNormalTexture(VGTexture input, const ui32v2& dims, const vg::SamplerState& samplerState)
{
   LOG_WARN("Generating uncompressed normal texture");
   glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
   glViewport(0, 0, dims.x, dims.y);

   glDrawBuffer(GL_COLOR_ATTACHMENT0);

   VGTexture normalTexture;
   glCreateTextures(GL_TEXTURE_2D, 1, &normalTexture);

   ui32 mipLevels = static_cast<ui32>(std::floor(std::log2(std::max(dims.x, dims.y)))) + 1;
   // TODO: Combine specular into the alpha channel?
   // TODO: Remove alpha channel?
   glTextureStorage2D(normalTexture, mipLevels, GL_RGB8, dims.x, dims.y);

   glBindTextureUnit(0, input);
   glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, normalTexture, 0);

   mNormalProgram->use();
   vg::sBlendStates.REPLACE.set();
   glUniform4f(mUvRectUniform, 0.0f, 0.0f, 1.0f, 1.0f);
   glUniform1i(mTextureUniform, 0);
   glUniform2f(mPixelDimsUniform, 1.0f / dims.x, 1.0f / dims.y);
   sGlobalFullQuadVBO.draw();

   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   samplerState.setForTexture(normalTexture);
   glGenerateTextureMipmap(normalTexture);
   // Ensure texture writes have finished
   //glTextureBarrier(); We dont need this
   checkGlError("Generate Normal Maps End");

   vg::BlendState::restorePrevious();
   return normalTexture;
}

gli::texture2d MaterialTextureGenerator::generateAoRoughnessMetallicTexture(const gli::texture2d& ao, const gli::texture2d& roughness, const gli::texture2d& metallic, const ui32v2& dims, const vg::SamplerState& samplerState) {

    assert(ao.empty() || ao.format() == gli::format::FORMAT_R8_UNORM_PACK8);
    assert(roughness.empty() || roughness.format() == gli::format::FORMAT_R8_UNORM_PACK8);
    assert(metallic.empty() || metallic.format() == gli::format::FORMAT_R8_UNORM_PACK8);

    const ui32 pixelCount = dims.x * dims.y;
    gli::texture2d resultTexture(gli::FORMAT_RGB8_UNORM_PACK8, gli::texture2d::extent_type(dims.x, dims.y), 1);
    // These are separated into every possible case so we move all comparisons out of the critical
    // pixel loops to improve performance in debug mode
    // Also cache data ptr as the cast is a non trivial operation in debug mode
    ui8* targetData = resultTexture.data<ui8>();
    const ui8* aoData = ao.empty() ? nullptr : ao.data<ui8>();
    const ui8* metallicData = metallic.empty() ? nullptr : metallic.data<ui8>();
    const ui8* roughnessData = roughness.empty() ? nullptr : roughness.data<ui8>();
    if (aoData && roughnessData && metallicData) {
        for (ui32 i = 0; i < pixelCount; ++i) {
            const ui32 targetOffset = i * 3;
            targetData[targetOffset] = aoData[i];
            targetData[targetOffset + 1] = metallicData[i];
            targetData[targetOffset + 2] = roughnessData[i];
        }
    }
    else if (roughnessData && metallicData) {
        for (ui32 i = 0; i < pixelCount; ++i) {
            const ui32 targetOffset = i * 3;
            targetData[targetOffset] = UINT8_MAX;
            targetData[targetOffset + 1] = metallicData[i];
            targetData[targetOffset + 2] = roughnessData[i];
        }
    }
    else if (aoData && (!roughnessData && !metallicData)) {
        for (ui32 i = 0; i < pixelCount; ++i) {
            const ui32 targetOffset = i * 3;
            targetData[targetOffset] = aoData[i];
            targetData[targetOffset + 1] = UINT8_MAX;
            targetData[targetOffset + 2] = UINT8_MAX;
        }
    }
    else if (aoData && roughnessData && !metallicData) {
        for (ui32 i = 0; i < pixelCount; ++i) {
            const ui32 targetOffset = i * 3;
            targetData[targetOffset] = aoData[i];
            targetData[targetOffset + 1] = UINT8_MAX;
            targetData[targetOffset + 2] = roughnessData[i];
        }
    }
    else {
        for (ui32 i = 0; i < pixelCount; ++i) {
            const ui32 targetOffset = i * 3;
            targetData[targetOffset] = aoData ? aoData[i] : UINT8_MAX;
            targetData[targetOffset + 1] = metallicData ? metallicData[i] : UINT8_MAX;
            targetData[targetOffset + 2] = roughnessData ? roughnessData[i] : UINT8_MAX;
        }
    }

    return resultTexture;
}
