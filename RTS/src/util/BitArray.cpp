#include "stdafx.h"
#include "BitArray.h"

constexpr ui32 BITS_PER_ELEMENT = sizeof(ui8) * 8;

BitArray::BitArray()
{

}

BitArray::BitArray(ui32 numBits)
{
    resizeAndZero(numBits);
}

BitArray::~BitArray()
{

}

void BitArray::resize(ui32 numBits) {
    mData.resize(size_t((numBits + (BITS_PER_ELEMENT - 1)) / BITS_PER_ELEMENT));
}

void BitArray::resizeAndZero(ui32 numBits) {
    mData.resize(size_t((numBits + (BITS_PER_ELEMENT - 1)) / BITS_PER_ELEMENT), 0ui8);
}

void BitArray::setBit(ui32 index) {
    const ui32 i = index >> 3;
    const ui8 j = (ui8)(index - (i << 3));
    static_assert(BITS_PER_ELEMENT == 8);
    const ui8 bit = ((ui8)1 << j);
    mData[i] = mData[i] | bit;
}

void BitArray::clearBit(ui32 index) {
    const ui32 i = index >> 3;
    const ui8 j = (ui8)(index - (i << 3));
    static_assert(BITS_PER_ELEMENT == 8);
    mData[i] = (mData[i] & (~(1ui8 << j)));
}

void BitArray::setBitTo(ui32 index, bool val) {
    const ui32 i = index >> 3;
    const ui8 j = (ui8)(index - (i << 3));
    static_assert(BITS_PER_ELEMENT == 8);
    const ui8 bit = ((ui8)val << j);
    mData[i] = (mData[i] & (~(1ui8 << j))) | bit;
}

bool BitArray::getBit(ui32 index) const {
    const ui32 i = index >> 3;
    const ui32 j = index - (i << 3);
    static_assert(BITS_PER_ELEMENT == 8);
    return (mData[i] & (1 << j)) != 0;
}

void BitArray::zeroAllBits() {
    memset(mData.data(), 0, mData.size() * sizeof(ui8));
}

void BitArray::debugPrint(ui32 width, ui32 height) const
{
    for (ui32 y = 0; y < height; ++y) {
        printf("%2d| ", y);
        for (ui32 x = 0; x < width; ++x) {
            const ui32 index = y * width + x;
            assert(index < mData.size() / 8);
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
