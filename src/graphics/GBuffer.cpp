#include "Vorb/stdafx.h"
#include "Vorb/graphics/GBuffer.h"

#include "Vorb/graphics/SamplerState.h"

vg::GBuffer::GBuffer(ui32 w /*= 0*/, ui32 h /*= 0*/) :
m_size(w, h) {
    // Empty
}

void vg::GBuffer::initTarget(const ui32v2& _size, const ui32& texID, const vg::GBufferAttachment& attachment, int layerCount) {
    if (layerCount <= 1) {
        glBindTexture(GL_TEXTURE_2D, texID);
     //   if (glTexStorage2D) { // TODO: This doesnt work for manual mipmaps
     //       glTexStorage2D(GL_TEXTURE_2D, 1, (VGEnum)attachment.format, _size.x, _size.y);
     //   }
      //  else {
            glTexImage2D(GL_TEXTURE_2D, 0, (VGEnum)attachment.format, _size.x, _size.y, 0, (VGEnum)attachment.pixelFormat, (VGEnum)attachment.pixelType, nullptr);
     //   }
        vg::sSamplerStates.POINT_CLAMP.set(GL_TEXTURE_2D);
    }
    else {
        glBindTexture(GL_TEXTURE_2D_ARRAY, texID);
        if (glTexStorage3D) {
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, (VGEnum)attachment.format, _size.x, _size.y, layerCount);
        }
        else {
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, (VGEnum)attachment.format, _size.x, _size.y, layerCount, 0, (VGEnum)attachment.pixelFormat, (VGEnum)attachment.pixelType, nullptr);
        }
        vg::sSamplerStates.POINT_CLAMP.set(GL_TEXTURE_2D_ARRAY);
    }
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment.number, texID, 0);
    checkError();
}

vg::GBuffer& vg::GBuffer::init(const GBufferAttachment& geometryAttachment, const GBufferAttachment* normalAttachment, const GBufferAttachment* roughnessAttachment, int layerCount) {

    // Make the framebuffer
    glGenFramebuffers(1, &m_fboGeom);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);

    mLayerCount = layerCount;

    glGenTextures((GLsizei)1, &m_texGeom);
    initTarget(m_size, m_texGeom, geometryAttachment, layerCount);

    ui32 numAttachments = 1;
    if (normalAttachment) {
        glGenTextures((GLsizei)1, &m_texNormal);
        initTarget(m_size, m_texNormal, *normalAttachment, layerCount);
        ++numAttachments;
    }
    if (roughnessAttachment) {
        glGenTextures((GLsizei)1, &m_texRoughness);
        initTarget(m_size, m_texRoughness, *roughnessAttachment, layerCount);
        ++numAttachments;
    }
    // Add the attachments
    VGEnum bufs[3];
    for (ui32 i = 0; i < numAttachments; i++) {
        bufs[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    // Set the output location for pixels
    glDrawBuffers((GLsizei)numAttachments, bufs);

    // Unbind used resources
    if (layerCount > 1) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // TODO(Cristian): Change The Memory Usage Of The GPU

    return *this;
}
vg::GBuffer& vg::GBuffer::initDepth(TextureInternalFormat depthFormat /*= TextureInternalFormat::DEPTH_COMPONENT32*/, int layerCount) {
    assert(mLayerCount == layerCount);
    glGenTextures(1, &m_texDepth);
    if (layerCount <= 1) {
        glBindTexture(GL_TEXTURE_2D, m_texDepth);
        glTexImage2D(GL_TEXTURE_2D, 0, (VGEnum)depthFormat, m_size.x, m_size.y, 0, (VGEnum)vg::TextureFormat::DEPTH_COMPONENT, (VGEnum)vg::TexturePixelType::UNSIGNED_BYTE, nullptr);
        vg::sSamplerStates.POINT_CLAMP.set(GL_TEXTURE_2D);

        glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_texDepth, 0);

        checkError();

        // Unbind used resources
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else {
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_texDepth);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, (VGEnum)depthFormat, m_size.x, m_size.y, layerCount, 0, (VGEnum)vg::TextureFormat::DEPTH_COMPONENT, (VGEnum)vg::TexturePixelType::UNSIGNED_BYTE, nullptr);
        vg::sSamplerStates.POINT_CLAMP.set(GL_TEXTURE_2D_ARRAY);

        glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_texDepth, 0);

        checkError();

        // Unbind used resources
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    // TODO: Change The Memory Usage Of The GPU

    return *this;
}
vg::GBuffer& vg::GBuffer::initDepthStencil(TextureInternalFormat depthFormat /*= TextureInternalFormat::DEPTH24_STENCIL8*/) {
    glGenTextures(1, &m_texDepth);
    glBindTexture(GL_TEXTURE_2D, m_texDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, (VGEnum)depthFormat, m_size.x, m_size.y, 0, (VGEnum)vg::TextureFormat::DEPTH_STENCIL, (VGEnum)vg::TexturePixelType::UNSIGNED_INT_24_8, nullptr);
    vg::sSamplerStates.POINT_CLAMP.set(GL_TEXTURE_2D);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, m_texDepth, 0);

    // Unbind used resources
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return *this;
}
void vg::GBuffer::dispose() {
    if (m_fboGeom) {
        glDeleteFramebuffers(1, &m_fboGeom);
        m_fboGeom = 0;
    }
    if (m_texGeom) {
        glDeleteTextures(1, &m_texGeom);
        m_texGeom = 0;
    }
    if (m_texNormal) {
        glDeleteTextures(1, &m_texNormal);
        m_texNormal = 0;
    }
    if (m_texDepth) {
        glDeleteTextures(1, &m_texDepth);
        m_texDepth = 0;
    }
}

void vg::GBuffer::useGeometry() const {
    assert(m_fboGeom);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);
    glViewport(0, 0, m_size.x, m_size.y);
}

vorb::graphics::GBuffer::~GBuffer() {
    dispose();
}

void vorb::graphics::GBuffer::initMipLevelsGeom(const vg::GBufferAttachment& geomAttachment, int maxDepth /*= 0xff*/)
{
    assert(m_texGeom);
    assert(geomAttachment.number == FBO_GEOMETRY_COLOR);

    ui32 width = m_size.x / 2;
    ui32 height = m_size.y / 2;
    glBindTexture(GL_TEXTURE_2D, m_texGeom);
    for (mMipLevels = 1; mMipLevels <= maxDepth; ++mMipLevels) {
        assert(width > 0);
        // TODO: Compress (breaks normal generation so we need to post compress)
        if (mLayerCount <= 1) {
            glTexImage2D(GL_TEXTURE_2D, mMipLevels, (VGEnum)geomAttachment.format, width, height, 0, (VGEnum)geomAttachment.pixelFormat, (VGEnum)geomAttachment.pixelType, nullptr);
            checkError();
        }
        else {
            assert(false); // I dont think this is right
            glBindTexture(GL_TEXTURE_2D_ARRAY, m_texGeom);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, mMipLevels, (VGEnum)geomAttachment.format, width, height, mLayerCount, 0, (VGEnum)geomAttachment.pixelFormat, (VGEnum)geomAttachment.pixelType, nullptr);
            checkError();
        }
        if (width == 1 || height == 1) break;
        width = width / 2;
        height = height / 2;
    }
    glGenerateMipmap(GL_TEXTURE_2D); // TODO: is allocating images needed?
    glBindTexture(GL_TEXTURE_2D, 0);
}

void vorb::graphics::GBuffer::unuse() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool vorb::graphics::GBuffer::checkError() {
    const int fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        std::string errorString;
        switch (fboStatus) {
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_UNDEFINED:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                errorString = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
                break;
            case GL_INVALID_ENUM:
                errorString = "No framebuffer bound";
                break;
            default:
                errorString = "UNKNOWN - " + std::to_string(fboStatus);
                break;
        }
        printf("FBO Error: %s %d\n", errorString.c_str(), fboStatus);
        return true;
    }

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::string errorString;
        switch (error) {
            case GL_INVALID_ENUM:
                errorString = "Framebuffer init error. Error code 1280: GL_INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                errorString = "Framebuffer init error.  Error code 1281: GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                errorString = "Framebuffer init error.  Error code 1282: GL_INVALID_OPERATION";
                break;
            case GL_STACK_OVERFLOW:
                errorString = "Framebuffer init error.  Error code 1283: GL_STACK_OVERFLOW";
                break;
            case GL_STACK_UNDERFLOW:
                errorString = "Framebuffer init error.  Error code 1284: GL_STACK_UNDERFLOW";
                break;
            case GL_OUT_OF_MEMORY:
                errorString = "Framebuffer init error.  Error code 1285: GL_OUT_OF_MEMORY";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                errorString = "Framebuffer init error.  Error code 1285: GL_INVALID_FRAMEBUFFER_OPERATION";
                break;
            default:
                errorString = "Framebuffer init error.  Error code " + std::to_string(error) + ": UNKNOWN";
                break;
        }
        printf("FBO Error: %s %d\n", errorString.c_str(), (int)error);
        return true;
    }
    return false;

    return false;
}
void vg::GBuffer::bindGeometryTexture(ui32 textureUnit, GLenum target /*= GL_TEXTURE_2D*/) {
    assert(m_texGeom);
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(target, m_texGeom);
}
void vg::GBuffer::bindNormalTexture(ui32 textureUnit, GLenum target /*= GL_TEXTURE_2D*/) {
    assert(m_texNormal);
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(target, m_texNormal);
}
void vg::GBuffer::bindDepthTexture(ui32 textureUnit, GLenum target /*= GL_TEXTURE_2D*/) {
    assert(m_texDepth);
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(target, m_texDepth);
}