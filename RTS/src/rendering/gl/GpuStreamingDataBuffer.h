#pragma once
// Triple buffered data stream for uploading data to the GPU every frame
class GpuStreamingDataBuffer {
public:
    GpuStreamingDataBuffer(ui32 maxElements, ui32 elementSize);
    ~GpuStreamingDataBuffer();

    VORB_NON_COPYABLE(GpuStreamingDataBuffer);

    // Get start of location to copy data to
    void* frameBeginAndGetDataForUpdate();

    // Call to upload data and increment frame. Only once per frame
    // @elementCount is the number of elements we updated this frame
    // Returns the element index of the start of the SSBO to be uploaded as uniform
    int flushDataAndIncrementFrame(ui32 elementCount);

    void bindBufferAsSSBO(GLuint bindingPoint);
    void bindAsVertexArrayVertexBuffer(VGBuffer targetVao, GLuint bindingIndex, GLintptr offset, GLsizei stride);
    ui32 getMaxElements() const { return mMaxElements; }
    ui32 getByteOffsetLastFlush() const { return mByteOffsetLastFlush; }
    ui32 getCurrentElementOffset() const { return mFrameIndex * mMaxElements;}
    VGBuffer getBufferObject() const { return mBufferObject; }

private:
    void* mMappedBuffer;
    VGBuffer mBufferObject;
    int mFrameIndex = 0;
    ui32 mByteOffsetLastFlush = 0;
    ui32 mMaxElements;
    ui32 mElementSize;
    GLsync mFence[3] = { 0 };
};
