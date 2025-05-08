#pragma once
// Triple buffered data stream for uploading data to the GPU every frame
class GpuStreamingDataBuffer {
public:
    GpuStreamingDataBuffer(ui32 maxElements, ui32 elementSize);
    ~GpuStreamingDataBuffer();

    VORB_NON_COPYABLE(GpuStreamingDataBuffer);

    // Invalidates the buffer. Call before frameBegin
    void setMaxElements(ui32 maxElements);

    // Get start of location to copy data to
    void* frameBeginAndGetDataForUpdate();

    // Call to upload data and increment frame. Only once per frame
    // elementCount is the number of elements we updated this frame
    void flushDataAndIncrementFrame(ui32 elementCount);

    // Call AFTER flushing!
    void bindBufferAsSSBO(GLuint bindingPoint) const;
    // Call AFTER flushing!
    void bindAsVertexArrayVertexBuffer(VGBuffer targetVao, GLuint bindingIndex, GLintptr offset) const;
    ui32 getMaxElements() const { return mMaxElements; }
    ui32 getByteOffsetLastFlush() const { return mByteOffsetLastFlush; }
    VGBuffer getBufferObject() const { return mBufferObject; }

    // For example if fuzz is 64 it will not deallocate until the size is 64 less than the current capacity
    // Will use the first draw command buffer in the span and allocate all buffers to the same capacity
    static bool reallocateFuzzedIfNeeded(std::unique_ptr<GpuStreamingDataBuffer>& bufferPtr, size_t requiredCapacity, ui32 elementSize, size_t fuzz);

private:
    void initBuffer();

    void* mMappedBuffer;
    VGBuffer mBufferObject;
    i32 mFrameIndex = 0;
    ui32 mByteOffsetLastFlush = 0;
    ui32 mMaxElements;
    ui32 mElementSize;
    ui32 mTotalBytesLastFlush = 0;
    ui32 mFrameSizebytes = 0;
    GLsync mFence[3] = { 0 };

    inline static ui32 REQUIRED_ALIGNMENT = 32;
};
