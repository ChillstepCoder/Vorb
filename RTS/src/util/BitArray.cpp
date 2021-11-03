#include "stdafx.h"
#include "BitArray.h"

constexpr ui32 BITS_PER_ELEMENT = sizeof(ui8) * 8;

BitArray::BitArray()
{

}

BitArray::~BitArray()
{

}

void BitArray::resize(ui32 size) {
    mData.resize((size_t)size * BITS_PER_ELEMENT);
}

void BitArray::resizeAndZero(ui32 size) {
    mData.resize((size_t)size * BITS_PER_ELEMENT, 0ui8);
}

void BitArray::setBitTo(ui32 index, bool val) {
    const ui32 i = index >> 3;
    const ui32 j = index - (i << 3);
    static_assert(BITS_PER_ELEMENT == 8);
    mData[i] = (ui8)((ui32)val << j);
}

bool BitArray::getBit(ui32 index) {
    const ui32 i = index >> 3;
    const ui32 j = index - (i << 3);
    static_assert(BITS_PER_ELEMENT == 8);
    return (mData[i] & (1 << j)) > 0 ? true : false;
}

void BitArray::zeroAllBits() {
    memset(mData.data(), 0, mData.size() * BITS_PER_ELEMENT);
}
