#pragma once
// TODO: StaticBitARray
class BitArray
{
public:
    BitArray();
    BitArray(ui32 numBits);
    ~BitArray();

    void resize(ui32 numBits);
    void resizeAndZero(ui32 numBits);
    void setBit(ui32 index);
    void clearBit(ui32 index);
    void setBitTo(ui32 index, bool val);
    bool getBit(ui32 index) const;
    void zeroAllBits();
    void freeData() { std::vector<ui8>().swap(mData); }

    size_t getNumBits() const { return mData.size() * (sizeof(ui8) * 8u); }
    bool isEmpty() const { return mData.empty(); }

    void debugPrint(ui32 width, ui32 height) const;
private:
    std::vector<ui8> mData;
};

