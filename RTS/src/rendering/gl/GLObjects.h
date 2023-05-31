#pragma once


constexpr GLuint BUFFER_BASE_GLOBAL_UBO = 0; // Always bound
constexpr GLuint BUFFER_BASE_GLOBAL_MATERIAL_SSBO = 1; // Always bound
constexpr GLuint BUFFER_BASE_MESH_UBO = 2;
constexpr GLuint BUFFER_BASE_MESH_SSBO = 3;
constexpr GLuint BUFFER_BASE_GRASS_UBO = 9;
constexpr GLuint BUFFER_BASE_CAMERA_UBO = 10;

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
    GLBuffer(GLBuffer&& o) = delete;
    GLBuffer& operator=(GLBuffer&& o) = delete;

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
    VORB_NON_COPYABLE(GLIndirectBuffer);
    GLIndirectBuffer(GLIndirectBuffer&& o) = delete;
    GLIndirectBuffer& operator=(GLIndirectBuffer&& o) = delete;

    explicit GLIndirectBuffer(size_t maxDrawCommands)
        : mIndirectBuffer(sizeof(DrawElementsIndirectCommand) * maxDrawCommands, nullptr, GL_DYNAMIC_STORAGE_BIT)
        , mDrawCommands(maxDrawCommands)
    {}

    GLuint getHandle() const { return mIndirectBuffer.getHandle(); }
    void uploadIndirectBuffer();

    std::vector<DrawElementsIndirectCommand> mDrawCommands;

private:
    GLBuffer mIndirectBuffer;
    ui32 TMPlastUploadedSize = 0;
};