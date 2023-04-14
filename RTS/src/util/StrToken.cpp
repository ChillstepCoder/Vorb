#include "stdafx.h"
#include "StrToken.h"

StrToken::StrToken(const nString& str) : mToken(0ull) {
    size_t sz = str.size();
    assert(sz <= MAX_CHARS_IN_STRTOKEN_WITH_INDEX);
    size_t i = 0;
    for (; i < sz - 1; ++i) {
        mToken |= strTokenEncodeChar(str[i]) << (i * 5ull);
    }
    // Last can be either a char or an index
    ui64 c = strTokenEncodeChar(str[i]);
    if (c != 0) {
        mToken |= c << (i * 5ull);
    }
    else {
        // Try index
        mToken |= strTokenEncodeIndex(str[i]) << 60;
    }
}

void StrToken::toString(OUT char* outStr, OUT ui32* outLength) const {
    // Remove index
    ui64 val = mToken & (~TOKEN_INDEX_MASK);
    
    ui32 i = 0;
    for (; val != 0; ++i) {
        char c = char(val & 0x1full);
        outStr[i] = (c != 0 ? ('a' + c - 1) : '_');
        val >>= 5;
    }

    // Encode optional index between 0-15 always as 2 digits so it sorts properly
    const ui32 index = getIndex();
    if (index) {
        outStr[i++] = char('0' + (index / 10));
        outStr[i++] = char('0' + (index % 10));
    }

    if (outLength) {
        *outLength = i;
    }
    outStr[i] = '\0';
}

nString StrToken::toString() const {
    nString buffer;
    buffer.resize(14);
    ui32 tmpLength;
    toString(buffer.data(), &tmpLength);
    buffer.resize(tmpLength);
    return buffer;
}
