#include "stdafx.h"
#include "ParticleSystemRenderer.h"

#include "ResourceManager.h"
#include "particles/ParticleSystem.h"
#include "particles/ParticleSystemManager.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialManager.h"

ParticleSystemRenderer::ParticleSystemRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer, const f32v2 & gbufferDims) :
    mResourceManager(resourceManager),
    mMaterialRenderer(materialRenderer),
    mGbufferDims(gbufferDims) {
    mFullQuadVbo.init();
}

ParticleSystemRenderer::~ParticleSystemRenderer() {

}

void ParticleSystemRenderer::renderParticleSystems(const Camera2D& camera, vg::GBuffer* activeGbuffer, bool renderLitSystems) {

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);

    const ParticleSystemManager& manager = mResourceManager.getParticleSystemManager();
    for (auto&& layerName : manager.mSystemLayerSortOrder) {

        auto&& it = manager.mParticleSystems.find(layerName.second);
        const ParticleSystemArray& systemArray = it->second;
        if (systemArray.empty()) continue;

        ParticleSystemData& systemData = systemArray[0]->mSystemData;

        vg::BlendState::set(systemData.blendState);

        bool isPostProcess = systemData.postMaterialName.size() > 0;
        if (isPostProcess) {
            vg::GBuffer gbuffer = getOrCreateFramebufferForParticleSystem(layerName.second);
            checkGlError("CreateParticleFramebuffer");

            // Share depth texture with main FBO
            gbuffer.useGeometry();
            glClear(GL_COLOR_BUFFER_BIT);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, activeGbuffer ? activeGbuffer->getDepthTexture() : 0, 0);
            checkGlError("AttachDepthParticle");
        }

        for (auto&& system : systemArray) {
            if (system->mSystemData.isEmissive != renderLitSystems) {
                renderParticleSystem(camera, *system);
            }
        }

        if (isPostProcess) {
            if (activeGbuffer) {
                activeGbuffer->useGeometry();
            }
            else {
                vg::GBuffer::unuse();
            }
            // TODO This copy + lookup is redundant and dumb
            vg::GBuffer gbuffer = getOrCreateFramebufferForParticleSystem(layerName.second);
            renderPostProcess(systemData, gbuffer);
        }

    }

    // TODO: needed?
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_POINT_SPRITE);
}

void ParticleSystemRenderer::renderParticleSystem(const Camera2D& camera, const ParticleSystem& particleSystem) {
    if (!particleSystem.mParticles.size()) {
        return;
    }
    const Material* material = mResourceManager.getMaterialManager().getMaterial(particleSystem.mSystemData.materialName);

    // Lazy mesh init
    if (!particleSystem.mVbo) {
        glGenVertexArrays(1, &particleSystem.mVao);
        glGenBuffers(1, &particleSystem.mVbo);
        glBindVertexArray(particleSystem.mVao);
        glBindBuffer(GL_ARRAY_BUFFER, particleSystem.mVbo);
        vg::GLProgram& program = material->mProgram;
        program.enableVertexAttribArrays();
        glVertexAttribPointer(program.getAttribute("vPosition"), 3, GL_FLOAT, false, sizeof(Particle), (void*)offsetof(Particle, mPosition));
        glVertexAttribPointer(program.getAttribute("vSize"), 1, GL_UNSIGNED_SHORT, false, sizeof(Particle), (void*)offsetof(Particle, mSize));

        if (const VGAttribute* attr = program.tryGetAttribute("vLifetime")) {
            glVertexAttribPointer(*attr, 1, GL_FLOAT, false, sizeof(Particle), (void*)offsetof(Particle, mLifeTime));
        }

        if (const VGAttribute* attr = program.tryGetAttribute("vVelocity")) {
            glVertexAttribPointer(program.getAttribute("vVelocity"), 3, GL_FLOAT, false, sizeof(Particle), (void*)offsetof(Particle, mVelocity));
        }
    }
    else {
        glBindVertexArray(particleSystem.mVao);
        glBindBuffer(GL_ARRAY_BUFFER, particleSystem.mVbo);
    }

    // Update mesh
    if (particleSystem.mDirty) {
        const unsigned bufferSizeBytes = particleSystem.mParticles.size() * sizeof(Particle);
        // TODO: Look up buffer object streaming https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming 
        // Orphan the buffer for speed
        glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, GL_STREAM_DRAW);
        // Set data
        glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, particleSystem.mParticles.data());
        particleSystem.mDirty = false;
    }

    assert(material);
    mMaterialRenderer.bindMaterialForRender(*material);

    material->mProgram.enableVertexAttribArrays();
    glDrawArrays(GL_POINTS, 0, (unsigned)particleSystem.mParticles.size());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

vg::GBuffer ParticleSystemRenderer::getOrCreateFramebufferForParticleSystem(const nString& name)
{
    auto&& it = mGBuffers.find(name);
    if (it != mGBuffers.end()) {
        return it->second;
    }

    vg::GBuffer newGBuffer;
    vg::GBufferAttachment attachments[1];
    // Color
    attachments[FBO_GEOMETRY_COLOR].format = vg::TextureInternalFormat::R8;
    attachments[FBO_GEOMETRY_COLOR].number = FBO_GEOMETRY_COLOR;
    attachments[FBO_GEOMETRY_COLOR].pixelFormat = vg::TextureFormat::RED;
    attachments[FBO_GEOMETRY_COLOR].pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    newGBuffer.setSize(ui32v2(mGbufferDims));
    newGBuffer.init(Array<vg::GBufferAttachment>(attachments, 1), vg::TextureInternalFormat::R8);
    //newGBuffer.initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT24);
    checkGlError("Particle GBuffer init");
    mGBuffers[name] = newGBuffer;
    return newGBuffer;
}

void ParticleSystemRenderer::renderPostProcess(const ParticleSystemData& particleSystemData, vg::GBuffer& gBuffer)
{
    const Material* material = mResourceManager.getMaterialManager().getMaterial(particleSystemData.postMaterialName);
    assert(material);

    mMaterialRenderer.bindMaterialForRender(*material);

    if (const VGUniform* inputUniform = material->mProgram.tryGetUniform("ParticleFbo")) {
        gBuffer.bindGeometryTexture(0, 0);
        glUniform1i(*inputUniform, 0);
    }

    if (const VGUniform* inputUniform = material->mProgram.tryGetUniform("unPixelDims")) {
        glUniform2f(*inputUniform, 1.0f / mGbufferDims.x, 1.0f / mGbufferDims.y);
    }

    vg::DepthState::NONE.set();
    vg::BlendState::set(particleSystemData.postBlendState);
    mFullQuadVbo.draw();

    vg::DepthState::NONE.set();
}
