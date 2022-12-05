#pragma once

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
    VORB_NON_COPYABLE_BUT_MOVABLE(GLBuffer);

    GLBuffer() = default;
    GLBuffer(GLsizeiptr size, const void* data, GLbitfield flags);
    ~GLBuffer();

    void allocate(GLsizeiptr size, const void* data, GLbitfield flags);
    void updateSubData(GLintptr offset, GLsizeiptr size, const void* data);
    void destroy();

    GLuint getHandle() const { return mHandle; }
    ui32 getCapacity() const { return mCapacity; }
    GLbitfield getFlags() const { return mFlags; }

private:
    GLuint mHandle = 0;
    ui32 mCapacity = 0;
    GLbitfield mFlags = 0;
};

class GLIndirectBuffer final
{
public:
    VORB_NON_COPYABLE_BUT_MOVABLE(GLIndirectBuffer);

    explicit GLIndirectBuffer(size_t maxDrawCommands)
        : mIndirectBuffer(sizeof(DrawElementsIndirectCommand)* maxDrawCommands, nullptr, GL_DYNAMIC_STORAGE_BIT)
        , mDrawCommands(maxDrawCommands)
    {}

    GLuint getHandle() const { return mIndirectBuffer.getHandle(); }
    void uploadIndirectBuffer();

    std::vector<DrawElementsIndirectCommand> mDrawCommands;

private:
    GLBuffer mIndirectBuffer;
};