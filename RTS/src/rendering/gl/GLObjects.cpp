#include "stdafx.h"
#include "GLObjects.h"

#include "rendering/gl/GL.h"

GLBuffer::GLBuffer(GLsizeiptr size, const void* data, GLbitfield flags) {
    GL.glCreateBuffers(1, &mHandle);
    GL.glNamedBufferStorage(mHandle, size, data, flags);
    mCapacity = size;
    mFlags = flags;
}

GLBuffer::~GLBuffer() {
    GL.glDeleteBuffers(1, &mHandle);
}

inline void reallocateBuffer(GLuint& handle, GLsizeiptr size, const void* data, GLbitfield flags) {
    GL.glDeleteBuffers(1, &handle);
    GL.glCreateBuffers(1, &handle);
    GL.glNamedBufferStorage(handle, size, data, flags);
}

void GLBuffer::allocate(GLsizeiptr size, const void* data, GLbitfield flags) {
    assert(size);
    if (mHandle == 0) {
        // Brand new buffer
        GL.glCreateBuffers(1, &mHandle);
        GL.glNamedBufferStorage(mHandle, size, data, flags);
    }
    else if (flags != mFlags) {
        // New flags always re-upload
        reallocateBuffer(mHandle, size, data, flags);
    }
    else if (mCapacity == size) {
        // No resize needed, just upload data if valid
        if (data) {
            if (mFlags & GL_DYNAMIC_STORAGE_BIT) {
                GL.glNamedBufferSubData(mHandle, (GLintptr)0, size, data);
            }
            else {
                // We cant use subdata without dynamic bit
                reallocateBuffer(mHandle, size, data, flags);
            }
        }
    }
    else {
        reallocateBuffer(mHandle, size, data, flags);
    }
    mCapacity = size;
    mFlags = flags;
}

void GLBuffer::updateSubData(GLintptr offset, GLsizeiptr size, const void* data) {
    assert(mHandle);
    assert(offset + size <= mCapacity);
    assert(mFlags & GL_DYNAMIC_STORAGE_BIT);
    GL.glNamedBufferSubData(mHandle, offset, size, data);
}

GLMappedBuffer::GLMappedBuffer(GLsizeiptr size, GLbitfield flags /*= GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT*/) : mFlags(flags) {
    glCreateBuffers(1, &mBufferObject);
    glNamedBufferStorage(mBufferObject, size, nullptr, flags);
    mMappedBuffer = glMapNamedBufferRange(mBufferObject, 0, size, flags | GL_MAP_FLUSH_EXPLICIT_BIT);
    assert(mMappedBuffer);
    mCapacity = size;
}

GLMappedBuffer::~GLMappedBuffer() {
    glUnmapNamedBuffer(mBufferObject);
    glDeleteBuffers(1, &mBufferObject);
}

void GLMappedBuffer::reallocate(GLsizeiptr size, GLbitfield flags /*= GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT*/) {
    assert(size);
    if (mBufferObject == 0) {
        // Brand new buffer
        GL.glCreateBuffers(1, &mBufferObject);
        GL.glNamedBufferStorage(mBufferObject, size, nullptr, flags);
        mMappedBuffer = glMapNamedBufferRange(mBufferObject, 0, size, flags | GL_MAP_FLUSH_EXPLICIT_BIT);
        mCapacity = size;
    }
    else if (flags != mFlags) {
        // New flags always re-upload
        reallocateInternal(size, flags);
    }
    else if (mCapacity < size) {
        reallocateInternal(size, flags);
    }
    else if (mCapacity * 2.0f > size) {
        // Shrink if we have halved our size
        reallocateInternal(size, flags);
    }
    mSize = size;
}

void GLMappedBuffer::reallocateInternal(GLsizeiptr size, GLbitfield flags) {
    GL.glDeleteBuffers(1, &mBufferObject);
    GL.glCreateBuffers(1, &mBufferObject);
    GL.glNamedBufferStorage(mBufferObject, size, nullptr, flags);
    mCapacity = size;
    mMappedBuffer = glMapNamedBufferRange(mBufferObject, 0, size, flags | GL_MAP_FLUSH_EXPLICIT_BIT);
    assert(mMappedBuffer);
    mFlags = flags;
}

void GLMappedBuffer::flushRange(GLintptr offsetBytes, GLsizeiptr sizeBytes) {
    glFlushMappedNamedBufferRange(mBufferObject, offsetBytes, sizeBytes);
}

void GLMappedBuffer::destroy() {
    glUnmapNamedBuffer(mBufferObject);
    glDeleteBuffers(1, &mBufferObject);
    mBufferObject = 0;
    mCapacity = 0;
    mMappedBuffer = nullptr;
}

void GLBuffer::destroy() {
    GL.glDeleteBuffers(1, &mHandle);
    mHandle = 0;
    mCapacity = 0;
}

void GLBuffer::bindAsVertexArrayVertexBuffer(VGBuffer targetVao, GLuint bindingIndex, GLintptr offset, GLsizei stride) {
    glVertexArrayVertexBuffer(targetVao, bindingIndex, mHandle, offset, stride);
}

void GLDrawCommandBuffer::uploadDrawCommands() {
    mIndirectBuffer.flushDataAndIncrementFrame(mNumActiveCommands);
}

void GLDrawCommandBuffer::multiDrawElementsIndirect(GLenum mode, GLenum type) const {
    GL.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, getHandle());
    void* byteOffset = (void*)getByteOffsetLastFlush();
    checkGlError("InstancedStaticModelRenderer::renderModelPass::B");
    glMultiDrawElementsIndirect(mode, type, byteOffset, (GLsizei)mNumActiveCommands, 0);
    checkGlError("InstancedStaticModelRenderer::renderModelPass::C");
}

bool GLDrawCommandBuffer::reallocateFuzzedIfNeeded(std::unique_ptr<GLDrawCommandBuffer>& bufferPtr, size_t requiredCapacity, size_t fuzz) {
    if (!bufferPtr || bufferPtr->getCapacity() < requiredCapacity) {
        bufferPtr = std::make_unique<GLDrawCommandBuffer>(requiredCapacity + fuzz / 2);
        return true;
    }
    else if (bufferPtr->getCapacity() > requiredCapacity + fuzz) {
        bufferPtr = std::make_unique<GLDrawCommandBuffer>(requiredCapacity + fuzz / 2);
        return true;
    }
    return false;
}
