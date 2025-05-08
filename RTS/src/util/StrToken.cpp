#include "stdafx.h"
#include "StrToken.h"

#include "serialization/NetSerialize.h"

// Must match sStrtokenEncodeTable
inline constexpr const char sStrtokenDecodeTable[64] = {
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
    ':',  // 31
    ';',  // 32
    '0',  // 33
    '1',  // 34
    '2',  // 35
    '3',  // 36
    '4',  // 37
    '5',  // 38
    '6',  // 39
    '7',  // 40
    '8',  // 41
    '9',  // 42
    '#',  // 43
    '!',  // 44
    '$',  // 45
    '%',  // 46
    '&',  // 47
    '*',  // 48
    '+',  // 49
    '<',  // 50
    '=',  // 51
    '>',  // 52
    '?',  // 53
    ',',  // 54
    '~',  // 55
    '(',  // 56
    ')',  // 57
    '[',  // 58
    ']',  // 59
    '{',  // 60
    '}',  // 61
    '|',  // 62
    '_',  // 63
};

StrToken::StrToken(const nString& str) : mTokenLow(0ull), mTokenMid(0ull), mTokenHigh(0ull) {
    initFromStrInternal(str.data(), str.size());
}

StrToken::StrToken(const char* str) : mTokenLow(0ull), mTokenMid(0ull), mTokenHigh(0ull) {
    initFromStrInternal(str, strlen(str));
}

void StrToken::toString(OUT char* outStr, OUT ui32* outLength) const {
    // Remove index
    ui64 valHigh = mTokenHigh;
    ui64 valMid = mTokenMid;
    ui64 valLow = mTokenLow;
    
    ui32 i = 0;
    for (; valLow != 0; ++i) {
        char c = char(valLow & 0x3full);
        outStr[i] = sStrtokenDecodeTable[c];
        valLow >>= 6;
    }
    for (; valMid != 0; ++i) {
        char c = char(valMid & 0x3full);
        outStr[i] = sStrtokenDecodeTable[c];
        valMid >>= 6;
    }
    for (; valHigh != 0; ++i) {
        char c = char(valHigh & 0x3full);
        outStr[i] = sStrtokenDecodeTable[c];
        valHigh >>= 6;
    }

    // Trim trailing underscores
    while (i > 0 && outStr[i - 1] == '_') {
        --i;
    }

    if (outLength) {
        *outLength = i;
    }
    outStr[i] = '\0';
}

nString StrToken::toString() const {
    nString buffer;
    buffer.resize(MAX_CHARS_IN_STRTOKEN);
    ui32 tmpLength;
    toString(buffer.data(), &tmpLength);
    buffer.resize(tmpLength);
    return buffer;
}

NET_SERIALIZE_DEF(StrToken, 
    serialize_uint64(stream, mTokenLow);
    serialize_uint64(stream, mTokenMid);
    serialize_uint64(stream, mTokenHigh);
)

void StrToken::initFromStrInternal(const char* str, size_t sz) {
    assert(sz <= MAX_CHARS_IN_STRTOKEN);
    size_t charIterMax = glm::min(sz, (ui64)MAX_CHARS_IN_STRTOKEN);
    size_t i = 0;
    // Encode low bytes
    for (; i < charIterMax && i < 10; ++i) {
        mTokenLow |= strTokenEncodeChar(str[i]) << (i * 6ull);
    }
    // Encode mid bytes
    for (; i < charIterMax && i < 20; ++i) {
        mTokenMid |= strTokenEncodeChar(str[i]) << ((i - 10) * 6ull);
    }
    // Encode high bytes
    for (; i < charIterMax; ++i) {
        mTokenHigh |= strTokenEncodeChar(str[i]) << ((i - 20) * 6ull);
    }
#ifdef DEBUG
    static thread_local UnorderedFlatMap<StrToken, nString> sStrTokenMap;
    auto&& it = sStrTokenMap.find(*this);
    if (it != sStrTokenMap.end()) {
        DEBUG_STR = it->second.data();
    }
    else {
        sStrTokenMap[*this] = toString();
        DEBUG_STR = sStrTokenMap[*this].data();
    }
#endif
}
