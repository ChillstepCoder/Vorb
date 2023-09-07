#include "stdafx.h"
#include "StrToken.h"


StrToken::StrToken(const nString& str) : mTokenHigh(0ull), mTokenLow(0ull) {
    size_t sz = str.size();
    assert(sz <= MAX_CHARS_IN_STRTOKEN_WITH_INDEX);
    size_t charIterMax = glm::min(sz, (ui64)MAX_CHARS_IN_STRTOKEN);
    size_t i = 0;
    // Encode low bytes
    for (; i < charIterMax && i < 12; ++i) {
        char c = str[i];
        if (c >= '0' && c <= '9') {
            break;
        }
        mTokenLow |= strTokenEncodeChar(c) << (i * 5ull);
    }
    // Encode high bytes
    for (; i < charIterMax; ++i) {
        char c = str[i];
        if (c >= '0' && c <= '9') {
            break;
        }
        mTokenHigh |= strTokenEncodeChar(c) << ((i - 12) * 5ull);
    }
    // Encode index
    ui64 index = 0;
    if (str[i] != '\0') {
        index = _atoi64(&str[i]);
    }
    
    assert(index <= STRTOKEN_MAX_INDEX);
    index = glm::min(index, STRTOKEN_MAX_INDEX);
    setIndex(index);
}

void StrToken::toString(OUT char* outStr, OUT ui32* outLength) const {
    // Remove index
    ui64 valHigh = (mTokenHigh & (~STRTOKEN_INDEX_MASK));
    ui64 valLow = mTokenLow;
    
    ui32 i = 0;
    for (; valLow != 0; ++i) {
        char c = char(valLow & 0x1full);
        outStr[i] = (c != 0 ? ('a' + c - 1) : '_');
        valLow >>= 5;
    }
    for (; valHigh != 0; ++i) {
        char c = char(valHigh & 0x1full);
        outStr[i] = (c != 0 ? ('a' + c - 1) : '_');
        valHigh >>= 5;
    }

    // Trim trailing underscores
    while (i > 0 && outStr[i - 1] == '_') {
        --i;
    }

    // Include all zeroes 
    const ui32 index = getIndex();
    if (index) {
        char indexBuf[16];
        _itoa_s(index, indexBuf, 16, 10);
        int j = 0;
        for (int j = 0; j < 4; ++j) {
            if (indexBuf[j] == '\0') break;
            outStr[i++] = indexBuf[j];
        }
    }

    if (outLength) {
        *outLength = i;
    }
    outStr[i] = '\0';
}

nString StrToken::toString() const {
    nString buffer;
    buffer.resize(MAX_CHARS_IN_STRTOKEN_WITH_INDEX);
    ui32 tmpLength;
    toString(buffer.data(), &tmpLength);
    buffer.resize(tmpLength);
    return buffer;
}
