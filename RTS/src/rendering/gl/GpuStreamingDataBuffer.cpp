#include "stdafx.h"
#include "GpuStreamingDataBuffer.h"

static bool sDidInitAlignment = false;

GpuStreamingDataBuffer::GpuStreamingDataBuffer(ui32 maxElements, ui32 elementSize) :
    mMaxElements(maxElements),
    mElementSize(elementSize) {
    ASSERT_RENDER_THREAD();

    assert(mMaxElements > 0 && mElementSize > 0);

    if (!sDidInitAlignment) {
        GLint required;
        glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, (GLint*)&required);
        REQUIRED_ALIGNMENT = (ui32)required;
        sDidInitAlignment = true;
    }

    initBuffer();
}

GpuStreamingDataBuffer::~GpuStreamingDataBuffer() {
    glUnmapNamedBuffer(mBufferObject);
    glDeleteBuffers(1, &mBufferObject);
}

void GpuStreamingDataBuffer::setMaxElements(ui32 maxElements) {
    mMaxElements = maxElements;
    mFrameSizebytes = mMaxElements * mElementSize;
    const ui32 offsetFromAligned = mFrameSizebytes % REQUIRED_ALIGNMENT;
    if (offsetFromAligned != 0) {
        mFrameSizebytes += REQUIRED_ALIGNMENT - (offsetFromAligned);
    }
    const size_t bufferSize = mFrameSizebytes * 3;

    glUnmapNamedBuffer(mBufferObject);
    glDeleteBuffers(1, &mBufferObject);
    initBuffer();
}

void* GpuStreamingDataBuffer::frameBeginAndGetDataForUpdate() {
    // Wait for the GPU to finish with this section of the buffer
    // Zero fence means either first frame or we didn't flush last frame (due to culling likely)
    if (mFence[mFrameIndex] != 0) {
        while (glClientWaitSync(mFence[mFrameIndex], 0, GL_TIMEOUT_IGNORED) == GL_TIMEOUT_EXPIRED) {
            // Keep waiting
        }
        glDeleteSync(mFence[mFrameIndex]);
        mFence[mFrameIndex] = 0;
    }

    const int bufferOffsetBytes = mFrameIndex * mFrameSizebytes;
    return (void*)&((ui8*)mMappedBuffer)[bufferOffsetBytes];
}

void GpuStreamingDataBuffer::flushDataAndIncrementFrame(ui32 elementCount) {
    assert(elementCount <= mMaxElements);
    elementCount = glm::min(elementCount, mMaxElements);
    mByteOffsetLastFlush = mFrameIndex * mFrameSizebytes;
    mTotalBytesLastFlush = elementCount * mElementSize;
    glFlushMappedNamedBufferRange(mBufferObject, mByteOffsetLastFlush, mTotalBytesLastFlush);

    mFence[mFrameIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    incrementMod3(mFrameIndex);
}

void GpuStreamingDataBuffer::bindBufferAsSSBO(GLuint bindingPoint) const {
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, bindingPoint, mBufferObject, mByteOffsetLastFlush, mTotalBytesLastFlush);
}

void GpuStreamingDataBuffer::bindAsVertexArrayVertexBuffer(VGBuffer targetVao, GLuint bindingIndex, GLintptr offset) const {
    glVertexArrayVertexBuffer(targetVao, bindingIndex, mBufferObject, mByteOffsetLastFlush + offset, mElementSize);
}

bool GpuStreamingDataBuffer::reallocateFuzzedIfNeeded(
    std::unique_ptr<GpuStreamingDataBuffer>& bufferPtr, size_t requiredCapacity, ui32 elementSize, size_t fuzz
) {
    if (!bufferPtr) {
        bufferPtr = std::make_unique<GpuStreamingDataBuffer>(requiredCapacity + fuzz / 2, elementSize);
        return true;
    }
    else if (bufferPtr->getMaxElements() < requiredCapacity || bufferPtr->getMaxElements() > requiredCapacity + fuzz) {
        // Grow or shrink if needed
        bufferPtr = std::make_unique<GpuStreamingDataBuffer>(requiredCapacity + fuzz / 2, elementSize);
        return true;
    }
    return false;
}

void GpuStreamingDataBuffer::initBuffer()
{
    ASSERT_RENDER_THREAD();

    // Allocate buffer and add padding bytes if not multiple of REQUIRED_ALIGNMENT
    mFrameSizebytes = mMaxElements * mElementSize;
    const ui32 offsetFromAligned = mFrameSizebytes % REQUIRED_ALIGNMENT;
    if (offsetFromAligned != 0) {
        mFrameSizebytes += REQUIRED_ALIGNMENT - (offsetFromAligned);
    }

    const size_t bufferSize = mFrameSizebytes * 3;
    glCreateBuffers(1, &mBufferObject);
    glNamedBufferStorage(mBufferObject, bufferSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    mMappedBuffer = glMapNamedBufferRange(mBufferObject, 0, bufferSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
}
