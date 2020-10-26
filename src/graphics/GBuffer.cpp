#include "Vorb/stdafx.h"
#include "Vorb/graphics/GBuffer.h"

#include "Vorb/graphics/SamplerState.h"

vg::GBuffer::GBuffer(ui32 w /*= 0*/, ui32 h /*= 0*/) :
m_size(w, h) {
    // Empty
}

void vg::GBuffer::initTarget(const ui32v2& _size, const ui32& texID, const vg::GBufferAttachment& attachment) {
    glBindTexture(GL_TEXTURE_2D, texID);
    if (glTexStorage2D) {
        glTexStorage2D(GL_TEXTURE_2D, 1, (VGEnum)attachment.format, _size.x, _size.y);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, (VGEnum)attachment.format, _size.x, _size.y, 0, (VGEnum)attachment.pixelFormat, (VGEnum)attachment.pixelType, nullptr);
    }
    SamplerState::POINT_CLAMP.set(GL_TEXTURE_2D);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment.number, GL_TEXTURE_2D, texID, 0);
}

vg::GBuffer& vg::GBuffer::init(const Array<GBufferAttachment>& attachments, vg::TextureInternalFormat lightFormat) {
    // Create texture targets
    if (lightFormat != vg::TextureInternalFormat::NONE) {
        m_textures.setData(attachments.size() + 1);
    }
    else {
        // No light storage
        m_textures.setData(attachments.size());
    }
    glGenTextures((GLsizei)m_textures.size(), &m_textures[0]);

    // Make the framebuffer
    glGenFramebuffers(1, &m_fboGeom);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);

    // Add the attachments
    VGEnum* bufs = (VGEnum*)alloca(attachments.size() * sizeof(VGEnum));
    for (ui32 i = 0; i < attachments.size(); i++) {
        bufs[i] = GL_COLOR_ATTACHMENT0 + attachments[i].number;
        initTarget(m_size, m_textures[i], attachments[i]);
    }

    // Set the output location for pixels
    glDrawBuffers((GLsizei)attachments.size(), bufs);

    // Make the framebuffer for lighting
    if (lightFormat != vg::TextureInternalFormat::NONE) {
        glGenFramebuffers(1, &m_fboLight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboLight);
        initTarget(m_size, m_textures[m_textures.size() - 1], { lightFormat, vg::TextureFormat::RGBA, vg::TexturePixelType::UNSIGNED_BYTE, 0 });
        checkError();
    }

    // Unbind used resources
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // TODO(Cristian): Change The Memory Usage Of The GPU

    return *this;
}
vg::GBuffer& vg::GBuffer::initDepth(TextureInternalFormat depthFormat /*= TextureInternalFormat::DEPTH_COMPONENT32*/) {
    glGenTextures(1, &m_texDepth);
    glBindTexture(GL_TEXTURE_2D, m_texDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, (VGEnum)depthFormat, m_size.x, m_size.y, 0, (VGEnum)vg::TextureFormat::DEPTH_COMPONENT, (VGEnum)vg::TexturePixelType::UNSIGNED_BYTE, nullptr);
    SamplerState::POINT_CLAMP.set(GL_TEXTURE_2D);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_texDepth, 0);

    checkError();

    // Unbind used resources
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // TODO: Change The Memory Usage Of The GPU

    return *this;
}
vg::GBuffer& vg::GBuffer::initDepthStencil(TextureInternalFormat depthFormat /*= TextureInternalFormat::DEPTH24_STENCIL8*/) {
    glGenTextures(1, &m_texDepth);
    glBindTexture(GL_TEXTURE_2D, m_texDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, (VGEnum)depthFormat, m_size.x, m_size.y, 0, (VGEnum)vg::TextureFormat::DEPTH_STENCIL, (VGEnum)vg::TexturePixelType::UNSIGNED_INT_24_8, nullptr);
    SamplerState::POINT_CLAMP.set(GL_TEXTURE_2D);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_texDepth, 0);

    if (m_fboLight) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboLight);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_texDepth, 0);
    }

    // Unbind used resources
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // TODO(Cristian): Change The Memory Usage Of The GPU

    return *this;
}
void vg::GBuffer::dispose() {
    // TODO(Cristian): Change The Memory Usage Of The GPU

    if (m_fboGeom) {
        glDeleteFramebuffers(1, &m_fboGeom);
        m_fboGeom = 0;
    }
    if (m_fboLight) {
        glDeleteFramebuffers(1, &m_fboLight);
        m_fboLight = 0;
    }
    if (m_textures.size()) {
        glDeleteTextures((GLsizei)m_textures.size(), &m_textures[0]);
        m_textures.setData(0);
    }
    if (m_texDepth) {
        glDeleteTextures(1, &m_texDepth);
        m_texDepth = 0;
    }
}

void vg::GBuffer::useGeometry() {
    assert(m_fboGeom);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboGeom);
    glViewport(0, 0, m_size.x, m_size.y);
}

void vg::GBuffer::useLight() {
    assert(m_fboLight);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboLight);
    glViewport(0, 0, m_size.x, m_size.y);
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
    return false;
}

void vg::GBuffer::bindGeometryTexture(size_t i, ui32 textureUnit) {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_textures[i]);
}
void vg::GBuffer::bindDepthTexture(ui32 textureUnit) {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_texDepth);
}
void vg::GBuffer::bindLightTexture(ui32 textureUnit) {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, getLightTexture());
}
