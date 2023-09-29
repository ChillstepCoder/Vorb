#pragma once

#include "util/StrtokenEncodeTable.h"

#include "serialization/YmlSerializer.h"

constexpr ui64 strTokenEncodeChar(const char c) {
    return (ui64)sStrtokenEncodeTable[c];
}

constexpr int MAX_CHARS_IN_STRTOKEN = 20;

// Constexpr 64 bit compressed lower case 21 character string with optional 4 digit integer at end
// For fast comparison and serialization
class StrToken
{
public:
    constexpr StrToken() : mTokenLow(0u), mTokenHigh(0u) {}
    explicit StrToken(const nString& str);

    // Guarenteed consteval initialization
    template<size_t N>
    explicit consteval StrToken(const char(&str)[N], bool DUMMY_FORCE_CONSTEXPR /*Allows us to select the consteval constructor*/) :
        mTokenLow(
            // Account for null terminator. Lenth of 1 is empty string
            ((N > 1 ? strTokenEncodeChar(str[0]) : 0ull)) |
            ((N > 2 ? strTokenEncodeChar(str[1]) : 0ull) << 6) |
            ((N > 3 ? strTokenEncodeChar(str[2]) : 0ull) << 12) |
            ((N > 4 ? strTokenEncodeChar(str[3]) : 0ull) << 18) |
            ((N > 5 ? strTokenEncodeChar(str[4]) : 0ull) << 24) |
            ((N > 6 ? strTokenEncodeChar(str[5]) : 0ull) << 30) |
            ((N > 7 ? strTokenEncodeChar(str[6]) : 0ull) << 36) |
            ((N > 8 ? strTokenEncodeChar(str[7]) : 0ull) << 42) |
            ((N > 9 ? strTokenEncodeChar(str[8]) : 0ull) << 48) |
            ((N > 10 ? strTokenEncodeChar(str[9]) : 0ull) << 54)
        ),
        mTokenHigh(
            ((N > 11 ? strTokenEncodeChar(str[10]) : 0ull)) |
            ((N > 12 ? strTokenEncodeChar(str[11]) : 0ull) << 6) |
            ((N > 13 ? strTokenEncodeChar(str[12]) : 0ull) << 12) |
            ((N > 14 ? strTokenEncodeChar(str[13]) : 0ull) << 18) |
            ((N > 15 ? strTokenEncodeChar(str[14]) : 0ull) << 24) |
            ((N > 16 ? strTokenEncodeChar(str[15]) : 0ull) << 30) |
            ((N > 17 ? strTokenEncodeChar(str[16]) : 0ull) << 36) |
            ((N > 18 ? strTokenEncodeChar(str[17]) : 0ull) << 42) |
            ((N > 19 ? strTokenEncodeChar(str[18]) : 0ull) << 48) |
            ((N > 20 ? strTokenEncodeChar(str[19]) : 0ull) << 54)

        ) 
    { static_assert(N <= MAX_CHARS_IN_STRTOKEN + 1 /*null terminator*/);
    UNUSED(DUMMY_FORCE_CONSTEXPR);
#ifdef DEBUG
    DEBUG_STR = str;
#endif
    }

    explicit StrToken(const char* str);
    explicit StrToken(const char* str, size_t len) : mTokenHigh(0ull), mTokenLow(0ull) {
        initFromStrInternal(str, len);
    }

    bool operator==(const StrToken& rhs) const {
        return mTokenLow == rhs.mTokenLow && mTokenHigh == rhs.mTokenHigh;
    }
    bool operator<(const StrToken& rhs) const {
        return mTokenLow < rhs.mTokenLow || (mTokenLow == rhs.mTokenLow && mTokenHigh < rhs.mTokenHigh);
    }

    // Buffer length must be at least MAX_CHARS_IN_STRTOKEN
    void toString(OUT char* outStr, OUT ui32* outLength) const;
    nString toString() const;

    bool isValid() const { return mTokenLow != 0ull || mTokenHigh != 0ull; }

    NET_SERIALIZE_DECL();

protected:
    ui64 mTokenLow;  // Lower 64 bits
    ui64 mTokenHigh; // Upper 64 bits
#ifdef DEBUG
    // Debug str only works for constexpr strings since we cant own the string data
    const char* DEBUG_STR = "EMPTY";
#endif
    friend struct std::hash<StrToken>;

    void initFromStrInternal(const char* str, size_t sz);

};
#ifdef DEBUG
static_assert(sizeof(StrToken) == 24);
#else
static_assert(sizeof(StrToken) == 16);
#endif

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
    c4::csubstr str;
    n >> str;
    if (str.size() > MAX_CHARS_IN_STRTOKEN) {
        panic("Invalid string token length (max 20) {} {}", str.size(), str.data());
    };
    *target = StrToken(str.data(), str.size());
    return true;
}

// Guarenteed consteval initialization
template<size_t N>
inline consteval StrToken CStrToken(const char(&str)[N]) {
    return StrToken(str, false /*Allows us to select the consteval constructor*/);
}