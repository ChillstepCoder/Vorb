#include "stdafx.h"
#include "GLObjects.h"

GLBuffer::GLBuffer(GLsizeiptr size, const void* data, GLbitfield flags)
{
    glCreateBuffers(1, &mHandle);
    glNamedBufferStorage(mHandle, size, data, flags);
}

GLBuffer::~GLBuffer()
{
    glDeleteBuffers(1, &mHandle);
}

void GLBuffer::allocate(GLsizeiptr size, const void* data, GLbitfield flags)
{
    assert(mHandle == 0);
    glCreateBuffers(1, &mHandle);
    glNamedBufferStorage(mHandle, size, data, flags);
}

void GLBuffer::destroy() {
    glDeleteBuffers(1, &mHandle);
    mHandle = 0;
}

void GLIndirectBuffer::uploadIndirectBuffer()
{
    glNamedBufferSubData(mIndirectBuffer.getHandle(), 0, sizeof(DrawElementsIndirectCommand) * mDrawCommands.size(), mDrawCommands.data());
}
