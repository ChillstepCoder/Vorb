#pragma once

#include "util/StrtokenEncodeTable.h"

#include "serialization/YmlSerializer.h"

constexpr ui64 strTokenEncodeChar(const char c) {
    return (ui64)sStrtokenEncodeTable[c];
}

constexpr ui64 STRTOKEN_INDEX_BITS = 0x1ff; // 9 bits
constexpr ui64 STRTOKEN_MAX_INDEX = STRTOKEN_INDEX_BITS;
constexpr ui64 STRTOKEN_INDEX_SHIFT = 55ull;
constexpr ui64 STRTOKEN_INDEX_MASK = STRTOKEN_INDEX_BITS << STRTOKEN_INDEX_SHIFT;
constexpr int MAX_CHARS_IN_STRTOKEN = 23; // Does not include index
constexpr int MAX_CHARS_IN_STRTOKEN_WITH_INDEX = 26;

// Constexpr 64 bit compressed lower case 22 character string with optional 4 digit integer at end
// For fast comparison and serialization
class StrToken
{
public:
    constexpr StrToken() : mTokenLow(0u), mTokenHigh(0u) {}

    template<size_t N>
    explicit constexpr StrToken(const char(&str)[N], ui64 index) :
        mTokenLow(
            ((N > 0 ? strTokenEncodeChar(str[0]) : 0ull)) |
            ((N > 1 ? strTokenEncodeChar(str[1]) : 0ull) << 5) |
            ((N > 2 ? strTokenEncodeChar(str[2]) : 0ull) << 10) |
            ((N > 3 ? strTokenEncodeChar(str[3]) : 0ull) << 15) |
            ((N > 4 ? strTokenEncodeChar(str[4]) : 0ull) << 20) |
            ((N > 5 ? strTokenEncodeChar(str[5]) : 0ull) << 25) |
            ((N > 6 ? strTokenEncodeChar(str[6]) : 0ull) << 30) |
            ((N > 7 ? strTokenEncodeChar(str[7]) : 0ull) << 35) |
            ((N > 8 ? strTokenEncodeChar(str[8]) : 0ull) << 40) |
            ((N > 9 ? strTokenEncodeChar(str[9]) : 0ull) << 45) |
            ((N > 10 ? strTokenEncodeChar(str[10]) : 0ull) << 50) |
            ((N > 11 ? strTokenEncodeChar(str[11]) : 0ull) << 55)
        ),
        mTokenHigh(
            ((N > 12 ? strTokenEncodeChar(str[12]) : 0ull)) |
            ((N > 13 ? strTokenEncodeChar(str[13]) : 0ull) << 5) |
            ((N > 14 ? strTokenEncodeChar(str[14]) : 0ull) << 10) |
            ((N > 15 ? strTokenEncodeChar(str[15]) : 0ull) << 15) |
            ((N > 16 ? strTokenEncodeChar(str[16]) : 0ull) << 20) |
            ((N > 17 ? strTokenEncodeChar(str[17]) : 0ull) << 25) |
            ((N > 18 ? strTokenEncodeChar(str[18]) : 0ull) << 30) |
            ((N > 19 ? strTokenEncodeChar(str[19]) : 0ull) << 35) |
            ((N > 20 ? strTokenEncodeChar(str[20]) : 0ull) << 40) |
            ((N > 21 ? strTokenEncodeChar(str[21]) : 0ull) << 45) |
            ((N > 22 ? strTokenEncodeChar(str[22]) : 0ull) << 50) |
            ((index & STRTOKEN_INDEX_BITS) << STRTOKEN_INDEX_SHIFT)

        ) { static_assert(N <= MAX_CHARS_IN_STRTOKEN); }

    explicit StrToken(const nString& str);
    explicit StrToken(const char* str);

    bool operator==(const StrToken& rhs) const {
        return mTokenLow == rhs.mTokenLow && mTokenHigh == rhs.mTokenHigh;
    }
    bool operator<(const StrToken& rhs) const {
        return mTokenLow < rhs.mTokenLow || (mTokenLow == rhs.mTokenLow && mTokenHigh < rhs.mTokenHigh);
    }

    // Buffer length must be at least 14
    void toString(OUT char* outStr, OUT ui32* outLength) const;
    nString toString() const;

    ui32 getIndex() const {
        return static_cast<ui32>((mTokenHigh & STRTOKEN_INDEX_MASK) >> STRTOKEN_INDEX_SHIFT);
    }

    void setIndex(ui32 index) {
        assert(index <= STRTOKEN_MAX_INDEX);
        mTokenHigh = (mTokenHigh & (~STRTOKEN_INDEX_MASK)) | ((ui64)index << STRTOKEN_INDEX_SHIFT);
    }

    bool isValid() const { return mTokenLow != 0ull || mTokenHigh != 0ull; }

    NET_SERIALIZE_DECL();

protected:
    ui64 mTokenLow;  // Lower 64 bits
    ui64 mTokenHigh; // Upper 64 bits
    friend struct std::hash<StrToken>;

    void initFromStrInternal(const char* str, size_t sz);

};

static_assert(sizeof(StrToken) == 16);

namespace std {
    template <>
    struct hash<StrToken> {
        auto operator()(const StrToken& token) const -> size_t {
            return hash<ui64>{}(token.mTokenLow) ^ (hash<ui64>{}(token.mTokenHigh));
        }
    };
}

YML_WRITE_DEF(StrToken) {
    ryml::NodeRef& nr = *n;
    nr << o.toString();
}
YML_READ_DEF(StrToken) {
    nString str;
    if (str.length() > MAX_CHARS_IN_STRTOKEN_WITH_INDEX) {
        return false;
    };
    n >> str;
    *target = StrToken(str);
    return true;
}