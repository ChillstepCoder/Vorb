#include "stdafx.h"
#include "StrToken.h"

#include "serialization/NetSerialize.h"

// Must match sStrtokenEncodeTable
inline constexpr const char sStrtokenDecodeTable[32] = {
    '_', // 0 (Invalid mapping)
    'a',  // 1
    'b',  // 2
    'c',  // 3
    'd',  // 4
    'e',  // 5
    'f',  // 6
    'g',  // 7
    'h',  // 8
    'i',  // 9
    'j',  // 10
    'k',  // 11
    'l',  // 12
    'm',  // 13
    'n',  // 14
    'o',  // 15
    'p',  // 16
    'q',  // 17
    'r',  // 18
    's',  // 19
    't',  // 20
    'u',  // 21
    'v',  // 22
    'w',  // 23
    'x',  // 24
    'y',  // 25
    'z',  // 26
    '@',  // 27
    '-',  // 28
    '.',  // 29
    '/',  // 30
    ':'   // 31
};

StrToken::StrToken(const nString& str) : mTokenHigh(0ull), mTokenLow(0ull) {
    initFromStrInternal(str.data(), str.size());
}

StrToken::StrToken(const char* str) : mTokenHigh(0ull), mTokenLow(0ull) {
    initFromStrInternal(str, strlen(str));
}

void StrToken::toString(OUT char* outStr, OUT ui32* outLength) const {
    // Remove index
    ui64 valHigh = (mTokenHigh & (~STRTOKEN_INDEX_MASK));
    ui64 valLow = mTokenLow;
    
    ui32 i = 0;
    for (; valLow != 0; ++i) {
        char c = char(valLow & 0x1full);
        outStr[i] = sStrtokenDecodeTable[c];
        valLow >>= 5;
    }
    for (; valHigh != 0; ++i) {
        char c = char(valHigh & 0x1full);
        outStr[i] = sStrtokenDecodeTable[c];
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
        // TODO: Prepend 0s?
        for (int j = 0; j < 3; ++j) {
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

NET_SERIALIZE_DEF(StrToken, 
    serialize_uint64(stream, mTokenLow);
    serialize_uint64(stream, mTokenHigh);
)

void StrToken::initFromStrInternal(const char* str, size_t sz) {
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

    index = glm::min(index, STRTOKEN_MAX_INDEX);
    setIndex(index);
}
