#include "stdafx.h"
#include "NormalMapGenerator.h"

#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/FullQuadVBO.h>
#include <Vorb/graphics/GLProgram.h>

const char* SIMPLE_VERT_SRC = R"(
in vec2 vPosition; // Position in screen space
uniform vec4 unUvRect;
out vec2 fUV;
void main() {
    vec2 subUV = (vPosition + 1.0) / 2.0;
    fUV = unUvRect.xy + unUvRect.zw * subUV;
    gl_Position = vec4(fUV * 2.0 - 1.0, 0, 1);
}
)";

const char* SIMPLE_FRAG_SRC = R"(
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

NormalMapGenerator::NormalMapGenerator() {

}

NormalMapGenerator::~NormalMapGenerator() {

}

void onError(Sender s, const nString& n) {
    pError("Failed to load internal normal map generator shader with error " + n);
}

void NormalMapGenerator::init() {
    glGenFramebuffers(1, &mFramebufferID);
    mProgram = std::make_unique<vg::GLProgram>();
    mProgram->onShaderCompilationError += makeDelegate(onError);
    mProgram->init();
    mProgram->addShader(vg::ShaderType::VERTEX_SHADER, SIMPLE_VERT_SRC);
    mProgram->addShader(vg::ShaderType::FRAGMENT_SHADER, SIMPLE_FRAG_SRC);
    mProgram->link();
    mProgram->initUniforms();

    mUvRectUniform = mProgram->getUniform("unUvRect");
    mTextureUniform = mProgram->getUniform("unTexture");
    mPixelDimsUniform = mProgram->getUniform("unPixelDims");

    checkGlError("NormalMapGenerator::init");
}

VGTexture NormalMapGenerator::generateNormalTexture(VGTexture input, const ui32v2& dims, const vg::SamplerState& samplerState)
{
   // Ensure texture writes have finished
   glTextureBarrier();
   glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
   glViewport(0, 0, dims.x, dims.y);

   glDrawBuffer(GL_COLOR_ATTACHMENT0);

   VGTexture normalTexture;
   glGenTextures(1, &normalTexture);
   glBindTexture(GL_TEXTURE_2D, normalTexture);
   samplerState.set(GL_TEXTURE_2D);

   ui32 mipLevels = static_cast<ui32>(std::floor(std::log2(std::max(dims.x, dims.y)))) + 1;
   // TODO: Combine specular into the alpha channel
   glTexStorage2D(GL_TEXTURE_2D, mipLevels, GL_RGBA8, dims.x, dims.y);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, input);
   glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, normalTexture, 0);

   mProgram->use();
   glUniform4f(mUvRectUniform, 0.0f, 0.0f, 1.0f, 1.0f);
   glUniform1i(mTextureUniform, 0);
   glUniform2f(mPixelDimsUniform, 1.0f / dims.x, 1.0f / dims.y);
   sGlobalFullQuadVBO.draw();

   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   // Ensure texture writes have finished
   glBindTexture(GL_TEXTURE_2D, normalTexture);
   glGenerateMipmap(GL_TEXTURE_2D);
   glTextureBarrier();
   checkGlError("Generate Normal Maps End");

   return normalTexture;
}
