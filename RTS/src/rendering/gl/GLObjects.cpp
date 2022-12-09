#include "stdafx.h"
#include "GLObjects.h"

GLBuffer::GLBuffer(GLsizeiptr size, const void* data, GLbitfield flags) {
    glCreateBuffers(1, &mHandle);
    glNamedBufferStorage(mHandle, size, data, flags);
    mCapacity = size;
    mFlags = flags;
}

GLBuffer::~GLBuffer() {
    glDeleteBuffers(1, &mHandle);
}

inline void reallocateBuffer(GLuint handle, GLsizeiptr size, const void* data, GLbitfield flags) {
    glDeleteBuffers(1, &handle);
    glCreateBuffers(1, &handle);
    glNamedBufferStorage(handle, size, data, flags);
    LOG_INFO("  Reallocate GLBuffer {} {}", handle, size);
}

void GLBuffer::allocate(GLsizeiptr size, const void* data, GLbitfield flags) {
    assert(size);
    if (mHandle == 0) {
        // Brand new buffer
        glCreateBuffers(1, &mHandle);
        glNamedBufferStorage(mHandle, size, data, flags);
        LOG_INFO("  Allocate GLBuffer {} {}", mHandle, size);
    }
    else if (flags != mFlags) {
        // New flags always re-upload
        reallocateBuffer(mHandle, size, data, flags);
    }
    else if (mCapacity == size) {
        // No resize needed, just upload data if valid
        if (data) {
            if (mFlags & GL_DYNAMIC_STORAGE_BIT) {
                glNamedBufferSubData(mHandle, (GLintptr)0, size, data);
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
    glNamedBufferSubData(mHandle, offset, size, data);
}

void GLBuffer::destroy() {
    glDeleteBuffers(1, &mHandle);
    mHandle = 0;
    mCapacity = 0;
}

void GLIndirectBuffer::uploadIndirectBuffer() {
    mIndirectBuffer.updateSubData(0, sizeof(DrawElementsIndirectCommand) * mDrawCommands.size(), mDrawCommands.data());
    TMPlastUploadedSize = mDrawCommands.size();
}
