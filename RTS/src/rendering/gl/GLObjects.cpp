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

void GLBuffer::destroy() {
    GL.glDeleteBuffers(1, &mHandle);
    mHandle = 0;
    mCapacity = 0;
}

void GLIndirectBuffer::uploadIndirectBuffer() {
    mIndirectBuffer.updateSubData(0, sizeof(DrawElementsIndirectCommand) * mDrawCommands.size(), mDrawCommands.data());
}