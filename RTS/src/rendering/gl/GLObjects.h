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
    GLBuffer(GLsizeiptr size, const void* data, GLbitfield flags);
    ~GLBuffer();

    GLuint getHandle() const { return handle_; }

private:
    GLuint handle_;
};

class GLIndirectBuffer final
{
public:
    explicit GLIndirectBuffer(size_t maxDrawCommands)
        : bufferIndirect_(sizeof(DrawElementsIndirectCommand)* maxDrawCommands, nullptr, GL_DYNAMIC_STORAGE_BIT)
        , drawCommands_(maxDrawCommands)
    {}

    GLuint getHandle() const { return bufferIndirect_.getHandle(); }
    void uploadIndirectBuffer()
    {
        glNamedBufferSubData(bufferIndirect_.getHandle(), 0, sizeof(DrawElementsIndirectCommand) * drawCommands_.size(), drawCommands_.data());
    }

    void selectTo(GLIndirectBuffer& buf, const std::function<bool(const DrawElementsIndirectCommand&)>& pred)
    {
        buf.drawCommands_.clear();
        for (const auto& c : drawCommands_)
        {
            if (pred(c))
                buf.drawCommands_.push_back(c);
        }
        buf.uploadIndirectBuffer();
    }

    std::vector<DrawElementsIndirectCommand> drawCommands_;

private:
    GLBuffer bufferIndirect_;
};