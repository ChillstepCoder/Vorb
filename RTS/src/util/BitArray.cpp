#include "stdafx.h"
#include "BitArray.h"

constexpr ui32 BITS_PER_ELEMENT = sizeof(BitArray::DataType) * 8;
constexpr ui32 HALF_BITS_PER_ELEMENT = BITS_PER_ELEMENT / 2;
constexpr ui32 ALL_BITS_SET = UINT32_MAX;
constexpr ui32 MOST_SIGNIFICANT_HALF_BITS_SET = 0xFFFF0000u;
constexpr ui32 BIT_SHIFT = 5; // equivalent to * or / BITS_PER_ELEMENT

BitArray::BitArray() = default;
BitArray::BitArray(ui32 numBits)
{
    resizeAndZero(numBits);
}

BitArray::~BitArray() = default;

void BitArray::resize(ui32 numBits) {
    mNumBits = numBits;
    mData.resize(size_t((numBits + (BITS_PER_ELEMENT - 1)) / BITS_PER_ELEMENT));
}

void BitArray::resizeAndZero(ui32 numBits) {
    mNumBits = numBits;
    mData.resize(size_t((numBits + (BITS_PER_ELEMENT - 1)) / BITS_PER_ELEMENT), 0);
}

void BitArray::push_back(bool val) {
    if ((mNumBits % BITS_PER_ELEMENT) == 0) [[unlikely]] {
        mData.push_back(0);
    }
    setBitTo(mNumBits, val);
    ++mNumBits;
}

void BitArray::pop_back() {
    assert(mNumBits);
    if ((--mNumBits % BITS_PER_ELEMENT) == 0) [[unlikely]] {
        mData.pop_back();
    }
}

void BitArray::fill(bool val) {
    memset(mData.data(), val ? 0xFF : 0, mData.size() * sizeof(DataType));
}

void BitArray::setBit(ui32 index) {
    const ui32 i = index >> BIT_SHIFT;
    const DataType j = (DataType)(index - (i << BIT_SHIFT));
    const DataType bit = ((DataType)1 << j);
    mData[i] = mData[i] | bit;
}

void BitArray::clearBit(ui32 index) {
    const ui32 i = index >> BIT_SHIFT;
    const DataType j = (DataType)(index - (i << BIT_SHIFT));
    mData[i] = (mData[i] & (~(1u << j)));
}

void BitArray::setBitTo(ui32 index, bool val) {
    const ui32 i = index >> BIT_SHIFT;
    const DataType j = (DataType)(index - (i << BIT_SHIFT));
    const DataType bit = ((DataType)val << j);
    mData[i] = (mData[i] & (~(1u << j))) | bit;
}

bool BitArray::getBit(ui32 index) const {
    const ui32 i = index >> BIT_SHIFT;
    const ui32 j = index - (i << BIT_SHIFT);
    return (mData[i] & (1u << j)) != 0;
}

void BitArray::zeroAllBits() {
    memset(mData.data(), 0, mData.size() * sizeof(DataType));
}
void BitArray::setAllBits() {
    memset(mData.data(), 0xff, mData.size() * sizeof(DataType));
}

ui32 BitArray::getIndexOfFirstSetBit(ui32 startIndex) const {
    ui32 startByte = startIndex >> BIT_SHIFT;
    assert(startByte < mData.size());
    if (mData[startByte] != 0) { // If its in the start byte and we have a start offset, check here first
        ui32 startBitOffset = startIndex - (startByte << BIT_SHIFT);
        if (startBitOffset) {
            // Skip first half bits if possible to speed up iteration
            if ((startBitOffset < HALF_BITS_PER_ELEMENT) && (((mData[startByte] << HALF_BITS_PER_ELEMENT) == 0))) startBitOffset = HALF_BITS_PER_ELEMENT;
            for (ui32 j = startBitOffset; j < BITS_PER_ELEMENT; ++j) {
                if (mData[startByte] & (1u << j)) {
                    return (startByte << BIT_SHIFT) + j;
                }
            }
            ++startByte;
        }
    }
    for (size_t i = startByte; i < mData.size(); ++i) {
        if (mData[i] != 0) {
            ui32 j = ((mData[i] << HALF_BITS_PER_ELEMENT) == 0) * HALF_BITS_PER_ELEMENT; // Skip first half bits if possible to speed up iteration
            for (; j < BITS_PER_ELEMENT; ++j) {
                if (mData[i] & (1u << j)) {
                    return (i << BIT_SHIFT) + j;
                }
            }
        }
    }
    return UINT32_MAX;
}

ui32 BitArray::getIndexOfFirstUnsetBit(ui32 startIndex) const {
    ui32 startByte = startIndex >> BIT_SHIFT;
    assert(startByte < mData.size());
    if (mData[startByte] != ALL_BITS_SET) { // If its in the start byte and we have a start offset, check here first
        ui32 startBitOffset = startIndex - (startByte << BIT_SHIFT);
        if (startBitOffset) {
            // Skip first half bits if possible to speed up iteration
            if ((startBitOffset < HALF_BITS_PER_ELEMENT) && (((mData[startByte] << HALF_BITS_PER_ELEMENT) == MOST_SIGNIFICANT_HALF_BITS_SET))) startBitOffset = HALF_BITS_PER_ELEMENT;
            for (ui32 j = startBitOffset; j < BITS_PER_ELEMENT; ++j) {
                if (!(mData[startByte] & (1u << j))) {
                    return (startByte << BIT_SHIFT) + j;
                }
            }
            ++startByte;
        }
    }
    for (size_t i = startByte; i < mData.size(); ++i) {
        if (mData[i] != ALL_BITS_SET) {
            ui32 j = ((mData[i] << HALF_BITS_PER_ELEMENT) == MOST_SIGNIFICANT_HALF_BITS_SET) * HALF_BITS_PER_ELEMENT; // Skip first half bits if possible to speed up iteration
            for (; j < BITS_PER_ELEMENT; ++j) {
                if (!(mData[i] & (1u << j))) {
                    return (i << BIT_SHIFT) + j;
                }
            }
        }
    }
    return UINT32_MAX;
}

void BitArray::setNOT(const BitArray& other) {
    mData.resize(other.mData.size());
    for (size_t i = 0; i < mData.size(); ++i) {
        mData[i] = ~other.mData[i];
    }
}

void BitArray::debugPrint(ui32 width, ui32 height) const {
    for (ui32 y = 0; y < height; ++y) {
        printf("%2d| ", y);
        for (ui32 x = 0; x < width; ++x) {
            const ui32 index = y * width + x;
            assert(index < mData.size() / BITS_PER_ELEMENT);
            printf("%2d ", (int)getBit(index));
        }
        std::cout << "\n";
    }
    std::cout << "  | ";
    for (ui32 x = 0; x < width; ++x) {
        printf("%2d ", x);
    }
    std::cout << std::endl;
}
