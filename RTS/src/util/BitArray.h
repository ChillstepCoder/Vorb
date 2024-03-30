#pragma once
// TODO: StaticBitARray
class BitArray
{
public:
    typedef ui32 DataType;

    BitArray();
    BitArray(ui32 numBits);
    ~BitArray();

    BitArray(const BitArray& o) = default;

    VORB_MOVABLE(BitArray);

    void resize(ui32 numBits);
    void resizeAndZero(ui32 numBits);
    void fill(bool val);
    void setBit(ui32 index);
    void clearBit(ui32 index);
    void setBitTo(ui32 index, bool val);
    bool getBit(ui32 index) const;
    void zeroAllBits();
    void setAllBits();
    void freeData() { std::vector<DataType>().swap(mData); }
    // Returns UINT32_MAX on failure
    ui32 getIndexOfFirstSetBit(ui32 startIndex) const;
    // Returns UINT32_MAX on failure
    ui32 getIndexOfFirstUnsetBit(ui32 startIndex) const;

    // Initialize to the other array with the bitwise ~
    void setNOT(const BitArray& other);

    size_t getNumBits() const { return mData.size() * (sizeof(DataType) * 8u); }
    bool isEmpty() const { return mData.empty(); }

    DataType* data() { return mData.data(); }
    const DataType* data() const { return mData.data(); }
    size_t getNumBytes() const { return mData.size() * sizeof(DataType); }

    void debugPrint(ui32 width, ui32 height) const;
private:
    std::vector<DataType> mData;
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
