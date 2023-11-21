#include "stdafx.h"
#include "ShadowRenderer.h"

#include "camera/Camera3D.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialShaderRepository.h"

#include "resources/IAssetRepository.h"

#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/graphics/SamplerState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/GBuffer.h>

#include "options/DebugOptions.h"

//#include "debugging/DebugRenderer.h"

constexpr int DEPTH_MAP_RESOLUTION = 2048; //4096


////===========================================================================
//static bool ShadowBoundsCalc(
//    const Coord2u& renderDims,
//    const CIGrFrustum& frustum,
//    float               density,
//    const Coord3f& dir,
//    const Coord3f& right,
//    const Coord3f& up,
//    Range2f* s,
//    Range2f* t,
//    Range2f* u,
//    float* clipDist
//) {
//    Coord3f shadowBasis[3] = { right, up, dir };
//
//    // Get frustum info
//    Coord4f viewZ;
//    float frustumLength;
//    IGrTransformGetFrustumInfo(frustum, &viewZ, &frustumLength);
//
//    // Compute view frustum bounds in light space
//    const Plane* planes;
//    const Coord3f* points;
//    IGrTransformGetFrustumGeometry(frustum, &planes, &points);
//    const Coord3f farPoint = 0.25f * (points[1] + points[2] + points[3] + points[4]);
//    const Coord3f nearPoint = points[GR_FRUSTUM_POINT_EYE];
//    const Coord3f viewDir = MathNormalizeHq(farPoint - nearPoint);
//
//    // Compute "clipDist" (0->1 range for how much of the view frustum the
//    // shadows cover) and shadow space view frustum AABB.  If we're given
//    // a shexel density then binary search for the best fitting clip distance.
//    if (density) {
//        float low = 0.0f, high = 1.0f;
//        while (high - low > 1.0f / 4096.0f) {
//            float mid = (high + low) * 0.5f;
//
//            *s = Range2f((float)HUGE_VAL, -(float)HUGE_VAL);
//            *t = Range2f((float)HUGE_VAL, -(float)HUGE_VAL);
//            *u = Range2f((float)HUGE_VAL, -(float)HUGE_VAL);
//            for (unsigned index = 0; index < GR_FRUSTUM_POINTS; ++index) {
//                Coord3f worldPos = points[index];
//                if (index)
//                    worldPos = points[0] + (worldPos - points[0]) * mid;
//
//                const float ds = MathDotProduct(worldPos, shadowBasis[0]);
//                const float dt = MathDotProduct(worldPos, shadowBasis[1]);
//                const float du = MathDotProduct(worldPos, shadowBasis[2]);
//                s->min = min(s->min, ds);
//                s->max = max(s->max, ds);
//                t->min = min(t->min, dt);
//                t->max = max(t->max, dt);
//                u->min = min(u->min, du);
//                u->max = max(u->max, du);
//            }
//
//            bool fits = (s->max - s->min <= renderDims.x * density) &&
//                (t->max - t->min <= renderDims.y * density);
//            if (fits)       // Continue between mid and high to tighten bounds
//                low = mid;
//            else            // Continue between low and mid
//                high = mid;
//        }
//        *clipDist = high;
//    }
//    else {
//        *clipDist = 1.0f;
//        *s = Range2f((float)HUGE_VAL, -(float)HUGE_VAL);
//        *t = Range2f((float)HUGE_VAL, -(float)HUGE_VAL);
//        *u = Range2f((float)HUGE_VAL, -(float)HUGE_VAL);
//        for (unsigned index = 0; index < GR_FRUSTUM_POINTS; ++index) {
//            const float ds = MathDotProduct(points[index], shadowBasis[0]);
//            const float dt = MathDotProduct(points[index], shadowBasis[1]);
//            const float du = MathDotProduct(points[index], shadowBasis[2]);
//            s->min = min(s->min, ds);
//            s->max = max(s->max, ds);
//            t->min = min(t->min, dt);
//            t->max = max(t->max, dt);
//            u->min = min(u->min, du);
//            u->max = max(u->max, du);
//        }
//    }
//
//    // Check for degenerate bounds
//    if (s->min >= s->max || t->min >= t->max || u->min >= u->max)
//        return false;
//
//    // Snap to shadow map texels to keep shadows stable
//    if (density) {
//        Coord3f center = Coord3f(s->min + s->max, t->min + t->max, u->min + u->max) * .5f;
//        center.x = floorf(center.x / density + .5f) * density;
//        center.y = floorf(center.y / density + .5f) * density;
//        center.z = floorf(center.z / density + .5f) * density;
//
//        s->min = center.x - density * renderDims.x / 2;
//        s->max = center.x + density * renderDims.x / 2;
//        t->min = center.y - density * renderDims.y / 2;
//        t->max = center.y + density * renderDims.y / 2;
//        u->min = center.z - density * renderDims.x / 2;
//        u->max = center.z + density * renderDims.x / 2;
//    }
//
//    return true;
//}



constexpr int MAX_MIP_LEVELS = 9; // TODO: Make this dynamic?

// TODO: https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-8-summed-area-variance-shadow-maps
// https://docs.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps

ShadowRenderer::ShadowRenderer(const ui32v2& gbufferDims) {

    {// Shadow map gbuffer

        mShadowMapGBuffer = std::make_unique<vg::GBuffer>(ui32v2(DEPTH_MAP_RESOLUTION), MAX_SHADOW_CASCADE_LEVELS + 1);
        // TODO: Testing RG16F to see if its fine
        mShadowMapGBuffer->initAttachment(vorb::graphics::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RG16F /*vg::TextureInternalFormat::RG32F*/, vg::sSamplerStates.LINEAR_CLAMP);

        // Set up additional params
        VGTexture shadowMapTexture = mShadowMapGBuffer->getAlbedoTexture();
        constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTextureParameterfv(shadowMapTexture, GL_TEXTURE_BORDER_COLOR, bordercolor);
        GLint maxAnisotropy = 0;
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
        glTextureParameteri(shadowMapTexture, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);

        // TODO: Experiment with lower depth resolution (depth16)
        mShadowMapGBuffer->initDepth(vg::GBufferDepthFormat::DEPTH_32, MAX_SHADOW_CASCADE_LEVELS + 1);
        checkGlError("Shadow FBO init");
    }

    // TODO: Can we compress depth size (alpha channel)
    {// Shadow mip gbuffer
        mShadowMipGBuffer = std::make_unique<vg::GBuffer>(gbufferDims);
        // TODO: Better sampler state?
        mShadowMipGBuffer->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGB16F, vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, MAX_MIP_LEVELS);

        checkGlError("Shadow mips init");
    }

    {// Shadow blur gbuffer
        for (int i = 0; i < 2; ++i) {
            mShadowBlurGBuffers[i] = std::make_unique<vg::GBuffer>(gbufferDims);
            mShadowBlurGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::R8, vg::sSamplerStates.LINEAR_CLAMP);
        }

        checkGlError("Shadow FBO 2 init");
    }

    // Materials
    mShadowMapperMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("shadow_mapper"));
    mShadowVarianceMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("shadow_variance"));
    mShadowApplyMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("shadow_apply"));
    mBlurMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("gaussian_blur_shadow"));
    mShadowMipMaterial = AssetUtil::addAssetToBundleAndGetUnloaded<MaterialShaderDef>(mShaderAssets, CStrToken("shadow_mipmap"));
}

constexpr f32 SUN_POSITION_UPDATE_THRESH_SQ = SQ(0.001f);
constexpr f32 CAMERA_POSITION_UPDATE_THRESH_SQ = SQ(1.0f);

void ShadowRenderer::beginFrame(const Camera3D& camera, const f32v3& sunPositionWorld) {

    f32 camZNear = camera.getZNear();
    f32 camZAngle = camera.getZAngle();
    {   // Check if we need to update this frame based on camera motion and time
        mLastCameraPos = camera.getPosition();
        mLastSunPosition = sunPositionWorld;

        if (glm::distance2(sunPositionWorld, mLastUpdatedSunPosition) < SUN_POSITION_UPDATE_THRESH_SQ &&
            glm::distance2(camera.getPosition(), mLastUpdatedCameraPos) < CAMERA_POSITION_UPDATE_THRESH_SQ &&
            camZAngle == mLastCamZAngle &&
            camZNear == mLastCamZNear &&
            sTotalTimeSeconds - mLastUpdateTime < sDebugOptions.mShadowUpdateRateSeconds) {
            mShouldUpdateShadowsThisFrame = false;
            return;
        }
        mLastUpdatedCameraPos = mLastCameraPos;
        mLastUpdatedSunPosition = mLastSunPosition;
        mLastCamZAngle = camZAngle;
        mLastCamZNear = camZNear;
        mLastUpdateTime = sTotalTimeSeconds;
        mShouldUpdateShadowsThisFrame = true;
    }

    mPlaneDistances[0] = camZNear + sDebugOptions.mShadowNearSize;
    mPlaneDistances[1] = mPlaneDistances[0] + 40.0f;
    mPlaneDistances[2] = mPlaneDistances[1] + 100.0f;
    mPlaneDistances[3] = mPlaneDistances[2] + 600.0f;
    const float zPlanes[MAX_SHADOW_CASCADE_LEVELS + 1] = {
        camZNear,
        mPlaneDistances[0],
        mPlaneDistances[1],
        mPlaneDistances[2],
        mPlaneDistances[3]
    };

    for (int i = 0; i < MAX_SHADOW_CASCADE_LEVELS; ++i) {
        const f32m4 proj = glm::perspective(
            glm::radians(camera.getFieldOfView()),
            camera.getAspectRatio(),
            zPlanes[i],
            zPlanes[i + 1]
        );
        updateFrustumCorners(proj, camera.getViewMatrix());

        f32v3 center(0);
        for (int i = 0; i < SHADOW_FRUSTUM_CORNER_COUNT; ++i) {
            center += f32v3(mFrustumCornersWorldSpace[i]);
        }
        center /= SHADOW_FRUSTUM_CORNER_COUNT;

        // View matrix
        f32m4 lightV = glm::lookAt(
            center + sunPositionWorld,
            center,
            f32v3(0.0f, 1.0f, 0.0f)
        );

        // Ortho projection matrix
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::min();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::min();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::min();
        for (int i = 0; i < SHADOW_FRUSTUM_CORNER_COUNT; ++i)
        {
            const f32v4 v = mFrustumCornersWorldSpace[i];
            const auto trf = lightV * v;
            minX = std::min(minX, trf.x);
            maxX = std::max(maxX, trf.x);
            minY = std::min(minY, trf.y);
            maxY = std::max(maxY, trf.y);
            minZ = std::min(minZ, trf.z);
            maxZ = std::max(maxZ, trf.z);
        }

        // Tune this parameter according to the scene
        const float zMult = sDebugOptions.mShadowZMult;
        if (minZ < 0) {
            minZ *= zMult;
        }
        else {
            minZ /= zMult;
        }
        if (maxZ < 0) {
            maxZ /= zMult;
        }
        else {
            maxZ *= zMult;
        }

        // Quantize positions to texel sizes to reduce flicker
        f32 xSpan = maxX - minX;
        f32 ySpan = maxY - minY;
        f32 scaleFactor = 256.0f;
        f32 xTexelSize = xSpan / (DEPTH_MAP_RESOLUTION / scaleFactor);
        f32 yTexelSize = ySpan / (DEPTH_MAP_RESOLUTION / scaleFactor);

        f32 offsetX = ceilf(camera.getPosition().x / xTexelSize);
        f32 offsetY = ceilf(camera.getPosition().y / yTexelSize);

        offsetX = (camera.getPosition().x / xTexelSize - offsetX) * xTexelSize;
        offsetY = (camera.getPosition().y / yTexelSize - offsetY) * yTexelSize;

        /*minX += camera.getPosition().x;
        minY += camera.getPosition().y;
        maxX += camera.getPosition().x;
        maxY += camera.getPosition().y;*/

        // Quantize scale (reduce flicker)
        //f32 scaleX = 2.0f / (maxX - minX);
        //f32 scaleY = 2.0f / (maxY - minY);
        //f32 scaleQuantizer = sDebugOptions.mShadowScaleQuantizer;
        //scaleX = 1.0f / ceilf(1.0f / scaleX * scaleQuantizer) * scaleQuantizer;
        //scaleY = 1.0f / ceilf(1.0f / scaleY * scaleQuantizer) * scaleQuantizer;

        //float offsetX = -0.5f * (maxX + minX) * scaleX; // Offset value for x dimension
        //float offsetY = -0.5f * (maxY + minY) * scaleY; // Offset value for y dimension

        //float halfTextureSize = 0.5f * DEPTH_MAP_RESOLUTION;
        //offsetX = ceilf(offsetX) / halfTextureSize;
        //offsetY = ceilf(offsetY) / halfTextureSize;/*
        /*minX -= camera.getPosition().x;
        maxX -= camera.getPosition().x;
        minY -= camera.getPosition().y;
        maxY -= camera.getPosition().y;
        minZ -= camera.getPosition().z;
        maxZ -= camera.getPosition().z;*/

      /*  minX -= offsetX;
        minY -= offsetY;
        maxX -= offsetX;
        maxY -= offsetY;
        if (i == 0) std::cout << minX << " " << maxX << " " << minY << " " << maxY << " " << minZ << " " << maxZ << std::endl;*/

        //// recalculate view based on our quantized position
        //center = f32v3((minX + maxX) * 0.5, (minY + maxY) * 0.5, 0.0f);
        //lightV = glm::lookAt(
        //    center + sunPositionWorld,
        //    center,
        //    f32v3(0.0f, 1.0f, 0.0f)
        //);
        offsetX = 0.0f;
        offsetY = 0.0f;

        // Pad for blending
        const int PADDING = 10;
        minX -= PADDING;
        maxX += PADDING;
        minY -= PADDING;
        maxY += PADDING;

        const f32m4 lightP = glm::ortho(minX + offsetX, maxX + offsetX, minY + offsetY, maxY + offsetY, minZ, maxZ);
        mLightVP[i] = lightP * lightV;
    }

    // Cache shader data
    mShaderData.shadowFrustumMatrices = getShadowFrustumMatrices();
    mShaderData.shadowCascadePlaneDistances = getShadowCascadePlaneDistances();
    mShaderData.shadowMap = getShadowMap();
}

void ShadowRenderer::useShadowBuffer() {
    assert(mShadowMapGBuffer->getFbo());
    glBindFramebuffer(GL_FRAMEBUFFER, mShadowMapGBuffer->getFbo());
    glViewport(0, 0, DEPTH_MAP_RESOLUTION, DEPTH_MAP_RESOLUTION);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

void ShadowRenderer::clearShadowTexture() {
    mShadowBlurGBuffers[0]->clearAttachment(vg::GBufferAttachmentIndex::ALBEDO);
}

void ShadowRenderer::renderShadows(const f32v3 cameraPos) {
    if (!mShaderAssets.areAllAssetsLoaded()) {
        return;
    }

    // Mip it
    mShadowMapGBuffer->bindAlbedoTexture(0);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    f32v3 offset = cameraPos - mLastUpdatedCameraPos;

    mShadowMipGBuffer->use();
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mShadowMipGBuffer->getAlbedoTexture(), 0);
    ui32 nextTextureIndex;
    MaterialRenderer::bindMaterialShaderForRender(*mShadowVarianceMaterial, &nextTextureIndex);
    glUniformMatrix4fv(mShadowVarianceMaterial->getUniform("unShadowFrustumMatrices[0]"), MAX_SHADOW_CASCADE_LEVELS, false, &(*getShadowFrustumMatrices())[0][0]);
    glUniform1fv(mShadowVarianceMaterial->getUniform("unShadowCascadePlaneDistances[0]"), MAX_SHADOW_CASCADE_LEVELS, getShadowCascadePlaneDistances());
    glUniform3fv(mShadowVarianceMaterial->getUniform("CameraOffset"), 1, &offset[0]);
    glBindTextureUnit(nextTextureIndex, getShadowMap());
    glUniform1i(mShadowVarianceMaterial->getUniform("unShadowMap"), nextTextureIndex++);

    // Need to store alpha as replace
    vg::BlendState::set(vorb::graphics::BlendStateType::REPLACE);

    sGlobalFullTriangleVAO.draw();

    generateMipmaps();

    vg::BlendState::restorePrevious();

    { // Apply shadows
        mShadowBlurGBuffers[0]->use();
        ui32 nextTextureIndex = 0;
        MaterialRenderer::bindMaterialShaderForRender(*mShadowApplyMaterial, &nextTextureIndex);

        mShadowMipGBuffer->bindAlbedoTexture(nextTextureIndex);
        glUniform1i(glGetUniformLocation(mShadowApplyMaterial->mProgram.getID(), "unShadowFbo"), nextTextureIndex);

        VGUniform mipCountUniform = glGetUniformLocation(mShadowApplyMaterial->mProgram.getID(), "unMipCount");
        ui32 mipCount = mShadowMipGBuffer->getNumMipLevels(vg::GBufferAttachmentIndex::ALBEDO);
        glUniform1i(mipCountUniform, mipCount);

        sGlobalFullTriangleVAO.draw();
    }

    // TODO: We should be blurring the mips
    blurShadowMap();
}

const VGTexture ShadowRenderer::getShadowMap() const {
    return mShadowMapGBuffer->getAlbedoTexture();
}

const f32 ShadowRenderer::getMaxDistance(ShadowLodDetail detail) const {
    return Shadows::getMaxDistance(mPlaneDistances, detail);
}

VGTexture ShadowRenderer::getShadowTexture() const {
    return mShadowBlurGBuffers[0]->getAlbedoTexture();
}

void ShadowRenderer::updateFrustumCorners(const f32m4& projection, const f32m4& view) {
    const auto inv = glm::inverse(projection * view);
    unsigned i = 0;
    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                const glm::vec4 pt =
                    inv * glm::vec4(
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        2.0f * z - 1.0f,
                        1.0f);
                mFrustumCornersWorldSpace[i++] = pt / pt.w;
            }
        }
    }
}

void ShadowRenderer::generateMipmaps() {

    // assert(IsBound(mShadowMIpGBuffer)

    ui32 nextTextureIndex = 0;

    MaterialRenderer::bindMaterialShaderForRender(*mShadowMipMaterial, &nextTextureIndex);
    VGUniform inputUniform = glGetUniformLocation(mShadowMipMaterial->mProgram.getID(), "unInputTexture");
    VGUniform levelUniform = glGetUniformLocation(mShadowMipMaterial->mProgram.getID(), "unPreviousLevel");
    VGTexture shadowMipTexture = mShadowMipGBuffer->getAlbedoTexture();
    vg::sSamplerStates.LINEAR_CLAMP.setForTexture(shadowMipTexture);
    glBindTextureUnit(nextTextureIndex, shadowMipTexture);
    glUniform1i(inputUniform, nextTextureIndex);

    ui32 mipCount = mShadowMipGBuffer->getNumMipLevels(vg::GBufferAttachmentIndex::ALBEDO);
    ui32 width = mShadowMipGBuffer->getSize().x / 2;
    ui32 height = mShadowMipGBuffer->getSize().y / 2;
    for (int i = 0; i < mipCount - 1; ++i) {
        glUniform1i(levelUniform, i);
        glTextureBarrier(); //TODO Can we eliminate these?
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mShadowMipGBuffer->getAlbedoTexture(), i + 1); // Write to next level
        GLenum buf = GL_COLOR_ATTACHMENT0;
        glDrawBuffers((GLsizei)1, &buf);
        glTextureBarrier();
        glViewport(0.0f, 0.0f, width, height);
        sGlobalFullTriangleVAO.draw();
        width /= 2;
        height /= 2;
    }
    checkGlError("Generate Shadow Mips");

}

void ShadowRenderer::blurShadowMap()
{
    ui32 nextTexture = 0;
    MaterialRenderer::bindMaterialShaderForRender(*mBlurMaterial, &nextTexture);

    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    const VGUniform& fboUniform = mBlurMaterial->mProgram.getUniform("unInputFbo");
    const VGUniform& dirUniform = mBlurMaterial->mProgram.getUniform("unDirection");
    for (int i = 0; i < sDebugOptions.mShadowBlurPasses; ++i) {

        // Horizontal
        mShadowBlurGBuffers[0]->bindAlbedoTexture(nextTexture);
        mShadowBlurGBuffers[1]->use();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, sDebugOptions.mShadowBlurRadius, 0.0f);
        sGlobalFullTriangleVAO.draw();

        // Vertical
        mShadowBlurGBuffers[1]->bindAlbedoTexture(nextTexture);
        mShadowBlurGBuffers[0]->use();
        glUniform1i(fboUniform, nextTexture);
        glUniform2f(dirUniform, 0.0f, sDebugOptions.mShadowBlurRadius);
        sGlobalFullTriangleVAO.draw();
    }
    vg::BlendState::restorePrevious();
}
