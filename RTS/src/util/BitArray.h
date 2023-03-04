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

    ui8* data() { return mData.data(); }
    const ui8* data() const { return mData.data(); }
    size_t getNumBytes() const { return mData.size(); }

    void debugPrint(ui32 width, ui32 height) const;
private:
    std::vector<ui8> mData;
};

template <size_t N>
class StaticBitArray {
    static_assert(N % 8 == 0, "We enforce byte alignment for simplicity");
public:
    void setBit(ui32 index);
    void clearBit(ui32 index);
    void setBitTo(ui32 index, bool val);
    bool getBit(ui32 index) const;
    void zeroAllBits();

    static size_t getNumBits() { return N; }
private:
    ui8 mData[N / 8] = {};
};

template <size_t N>
void StaticBitArray<N>::zeroAllBits() {
    memset(mData.data(), 0, N / 8);
}

template <size_t N>
bool StaticBitArray<N>::getBit(ui32 index) const {
    assert(index < N);
    const ui32 i = index >> 3;
    const ui32 j = index - (i << 3);
    return (mData[i] & (1 << j)) != 0;
}

template <size_t N>
void StaticBitArray<N>::setBitTo(ui32 index, bool val) {
    assert(index < N);
    const ui32 i = index >> 3;
    const ui8 j = (ui8)(index - (i << 3));
    const ui8 bit = ((ui8)val << j);
    mData[i] = (mData[i] & (~(1ui8 << j))) | bit;
}

template <size_t N>
void StaticBitArray<N>::clearBit(ui32 index) {
    assert(index < N);
    const ui32 i = index >> 3;
    const ui8 j = (ui8)(index - (i << 3));
    mData[i] &= (~(1ui8 << j));
}

template <size_t N>
void StaticBitArray<N>::setBit(ui32 index) {
    assert(index < N);
    const ui32 i = index >> 3;
    const ui8 j = (ui8)(index - (i << 3));
    const ui8 bit = ((ui8)1 << j);
    mData[i] |= bit;
}
