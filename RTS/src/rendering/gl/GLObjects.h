#pragma once

#include <span>

#include "rendering/gl/GpuStreamingDataBuffer.h"
#include "rendering/BufferBindingPoints.h"

struct DrawElementsIndirectCommand
{
    GLuint count_;
    GLuint instanceCount_;
    GLuint firstIndex_;
    GLuint baseVertex_;
    GLuint baseInstance_;
};

class GLBuffer
{
public:
    VORB_NON_COPYABLE(GLBuffer);

    GLBuffer() = default;
    GLBuffer(GLsizeiptr size, const void* data, GLbitfield flags);
    ~GLBuffer();

    GLBuffer(GLBuffer&& o) noexcept
        : mHandle(o.mHandle)
        , mCapacity(o.mCapacity)
        , mFlags(o.mFlags) {
        o.mHandle = 0;
        o.mCapacity = 0;
        o.mFlags = 0;
    }

    void allocate(GLsizeiptr size, const void* data, GLbitfield flags);
    void updateSubData(GLintptr offset, GLsizeiptr size, const void* data);
    void destroy();
    void bindAsVertexArrayVertexBuffer(VGBuffer targetVao, GLuint bindingIndex, GLintptr offset, GLsizei stride);

    GLuint getHandle() const { return mHandle; }
    ui32 getCapacity() const { return mCapacity; }
    GLbitfield getFlags() const { return mFlags; }

private:
    GLuint mHandle = 0;
    ui32 mCapacity = 0;
    GLbitfield mFlags = 0;
};

// TODO: This needs a fence sync or something! It causes flickering
class GLMappedBuffer
{
public:
    VORB_NON_COPYABLE(GLMappedBuffer);

    GLMappedBuffer() = default;
    // GL_MAP_FLUSH_EXPLICIT_BIT is always set
    GLMappedBuffer(GLsizeiptr size, GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    ~GLMappedBuffer();

    // GL_MAP_FLUSH_EXPLICIT_BIT is always set
    void reallocate(GLsizeiptr size, GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    void flushRange(GLintptr offsetBytes, GLsizeiptr sizeBytes);
    void destroy();
    void* getMappedBuffer() const { return mMappedBuffer; }

    GLuint getHandle() const { return mBufferObject; }
    ui32 getCapacity() const { return mCapacity; }
    GLbitfield getFlags() const { return mFlags; }

private:
    void reallocateInternal(GLsizeiptr size, GLbitfield flags);
    GLuint mBufferObject = 0;
    ui32 mCapacity = 0;
    ui32 mSize = 0;
    GLbitfield mFlags = 0;
    void* mMappedBuffer = nullptr;
};

class GLDrawCommandBuffer final
{
public:
    VORB_NON_COPYABLE(GLDrawCommandBuffer);
    GLDrawCommandBuffer(GLDrawCommandBuffer&& o) = delete;
    GLDrawCommandBuffer& operator=(GLDrawCommandBuffer&& o) = delete;

    explicit GLDrawCommandBuffer(size_t maxDrawCommands)
        : mIndirectBuffer(maxDrawCommands, sizeof(DrawElementsIndirectCommand)) {
    }

    // TODO: POOL ALLOCATE

    void frameBegin() {
        mDrawCommands =
            std::span<DrawElementsIndirectCommand>(
                (DrawElementsIndirectCommand*)mIndirectBuffer.frameBeginAndGetDataForUpdate(),
                mIndirectBuffer.getMaxElements()
            );
    }

    GLuint getHandle() const { return mIndirectBuffer.getBufferObject(); }
    // Capacity in number of draw commands
    ui32 getCapacity() const { return mIndirectBuffer.getMaxElements(); }
    // Call site is responsible for filling with valid commands and then calling setNumActiveCommands
    std::span<DrawElementsIndirectCommand> getDrawCommands() { return mDrawCommands; }

    // Call before uploadDrawCommands
    DrawElementsIndirectCommand& appendCommand() { assert(mNumActiveCommands < getCapacity()); return mDrawCommands[mNumActiveCommands++]; }
    void setNumActiveCommands(ui32 numActive) { assert(numActive <= getCapacity()); mNumActiveCommands = numActive; }
    void uploadDrawCommands();
    ui32 getByteOffsetLastFlush() const { return mIndirectBuffer.getByteOffsetLastFlush(); }

    ui32 getNumActiveCommands() const { return mNumActiveCommands; }

    void multiDrawElementsIndirect(GLenum mode, GLenum type) const;

private:
    std::span<DrawElementsIndirectCommand> mDrawCommands;
    GpuStreamingDataBuffer mIndirectBuffer;
    ui32 mNumActiveCommands = 0;
};