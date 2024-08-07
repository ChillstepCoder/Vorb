#include "stdafx.h"
#include "GpuStreamingDataBuffer.h"

GpuStreamingDataBuffer::GpuStreamingDataBuffer(ui32 maxElements, ui32 elementSize) :
    mMaxElements(maxElements),
    mElementSize(elementSize) {
    assert(mMaxElements > 0 && mElementSize > 0);

    initBuffer();
}

GpuStreamingDataBuffer::~GpuStreamingDataBuffer() {
    glUnmapNamedBuffer(mBufferObject);
    glDeleteBuffers(1, &mBufferObject);
}

void GpuStreamingDataBuffer::setMaxElements(ui32 maxElements) {
    mMaxElements = maxElements;
    const size_t bufferSize = mMaxElements * mElementSize * 3;

    glUnmapNamedBuffer(mBufferObject);
    glDeleteBuffers(1, &mBufferObject);
    initBuffer();
}

void* GpuStreamingDataBuffer::frameBeginAndGetDataForUpdate() {
    // Wait for the GPU to finish with this section of the buffer
    if (mFence[mFrameIndex] != 0) {
        while (glClientWaitSync(mFence[mFrameIndex], 0, GL_TIMEOUT_IGNORED) == GL_TIMEOUT_EXPIRED) {
            // Keep waiting
        }
        glDeleteSync(mFence[mFrameIndex]);
    }

    const int bufferOffsetBytes = mFrameIndex * mElementSize * mMaxElements;
    return (void*)&((ui8*)mMappedBuffer)[bufferOffsetBytes];
}

int GpuStreamingDataBuffer::flushDataAndIncrementFrame(ui32 elementCount) {
    assert(elementCount <= mMaxElements);
    elementCount = glm::min(elementCount, mMaxElements);
    mByteOffsetLastFlush = mFrameIndex * mElementSize * mMaxElements;

    glFlushMappedNamedBufferRange(mBufferObject, mByteOffsetLastFlush, elementCount * mElementSize);

    mFence[mFrameIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    const int bufferStartIndex = mFrameIndex * mMaxElements;

    incrementMod3(mFrameIndex);

    return bufferStartIndex;
}

void GpuStreamingDataBuffer::bindBufferAsSSBO(GLuint bindingPoint) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, mBufferObject);
}

void GpuStreamingDataBuffer::bindAsVertexArrayVertexBuffer(VGBuffer targetVao, GLuint bindingIndex, GLintptr offset, GLsizei stride) {
    glVertexArrayVertexBuffer(targetVao, bindingIndex, mBufferObject, offset, stride);
}

void GpuStreamingDataBuffer::initBuffer()
{
    const size_t bufferSize = mMaxElements * mElementSize * 3;

    glCreateBuffers(1, &mBufferObject);
    glNamedBufferStorage(mBufferObject, bufferSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    mMappedBuffer = glMapNamedBufferRange(mBufferObject, 0, bufferSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
}
