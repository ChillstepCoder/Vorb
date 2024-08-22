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
    // @elementCount is the number of elements we updated this frame
    // Returns the element offset of the start of the SSBO to be uploaded as uniform
    int flushDataAndIncrementFrame(ui32 elementCount);

    // To bind as SSBO you MUST pass in the element offset returned from flushDataAndIncrementFrame
    void bindBufferAsSSBO(GLuint bindingPoint);
    // To bind as vertex array vertex buffer you do not need an offset, as the mByteOffsetLastFlush will be added to offset param
    void bindAsVertexArrayVertexBuffer(VGBuffer targetVao, GLuint bindingIndex, GLintptr offset, GLsizei stride);
    ui32 getMaxElements() const { return mMaxElements; }
    ui32 getByteOffsetLastFlush() const { return mByteOffsetLastFlush; }
    ui32 getCurrentElementOffset() const { return mFrameIndex * mMaxElements;}
    VGBuffer getBufferObject() const { return mBufferObject; }

private:
    void initBuffer();

    void* mMappedBuffer;
    VGBuffer mBufferObject;
    int mFrameIndex = 0;
    ui32 mByteOffsetLastFlush = 0;
    ui32 mMaxElements;
    ui32 mElementSize;
    GLsync mFence[3] = { 0 };
};
