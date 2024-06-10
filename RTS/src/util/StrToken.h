#pragma once

#include "util/StrtokenEncodeTable.h"
#include <boost/functional/hash.hpp> // For boost::hash_combine

constexpr ui64 strTokenEncodeChar(const char c) {
    return (ui64)sStrtokenEncodeTable[c];
}

constexpr int MAX_CHARS_IN_STRTOKEN = 30;

const constexpr char* DEBUG_STR_EMPTY = "EMPTY";

// Constexpr 64 bit compressed lower case 20 character string
// For fast comparison and serialization
class StrToken
{
public:
    constexpr StrToken() : mTokenLow(0u), mTokenMid(0ull), mTokenHigh(0u) {}
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
        mTokenMid(
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
        ),
        mTokenHigh(
            ((N > 21 ? strTokenEncodeChar(str[20]) : 0ull)) |
            ((N > 22 ? strTokenEncodeChar(str[21]) : 0ull) << 6) |
            ((N > 23 ? strTokenEncodeChar(str[22]) : 0ull) << 12) |
            ((N > 24 ? strTokenEncodeChar(str[23]) : 0ull) << 18) |
            ((N > 25 ? strTokenEncodeChar(str[24]) : 0ull) << 24) |
            ((N > 26 ? strTokenEncodeChar(str[25]) : 0ull) << 30) |
            ((N > 27 ? strTokenEncodeChar(str[26]) : 0ull) << 36) |
            ((N > 28 ? strTokenEncodeChar(str[27]) : 0ull) << 42) |
            ((N > 29 ? strTokenEncodeChar(str[28]) : 0ull) << 48) |
            ((N > 30 ? strTokenEncodeChar(str[29]) : 0ull) << 54)
        )
    { static_assert(N <= MAX_CHARS_IN_STRTOKEN + 1 /*null terminator*/);
    UNUSED(DUMMY_FORCE_CONSTEXPR);
#ifdef DEBUG
    DEBUG_STR = str;
#endif
    }

    explicit StrToken(const char* str);
    explicit StrToken(const char* str, size_t len) : mTokenHigh(0ull), mTokenMid(0ull), mTokenLow(0ull) {
        initFromStrInternal(str, len);
    }
    explicit StrToken(std::string_view s) : mTokenHigh(0ull), mTokenMid(0ull), mTokenLow(0ull) {
        initFromStrInternal(s.data(), s.size());
    }

    bool operator==(const StrToken& rhs) const {
        return mTokenLow == rhs.mTokenLow && mTokenMid == rhs.mTokenMid && mTokenHigh == rhs.mTokenHigh;
    }
    bool operator<(const StrToken& rhs) const {
        if (mTokenLow != rhs.mTokenLow) return mTokenLow < rhs.mTokenLow;
        if (mTokenMid != rhs.mTokenMid) return mTokenMid < rhs.mTokenMid;
        return mTokenHigh < rhs.mTokenHigh;
    }

    // Buffer length must be at least MAX_CHARS_IN_STRTOKEN
    void toString(OUT char* outStr, OUT ui32* outLength) const;
    nString toString() const;

    bool isValid() const { return mTokenLow != 0ull || mTokenMid != 0ull || mTokenHigh != 0ull; }
    void clear() {
        mTokenLow = mTokenMid = mTokenHigh = 0ull;
#ifdef DEBUG
        DEBUG_STR = DEBUG_STR_EMPTY;
#endif
    }

    NET_SERIALIZE_DECL();

protected:
    ui64 mTokenLow;  // Lower 64 bits
    ui64 mTokenMid; // Upper 64 bits
    ui64 mTokenHigh; // Upper 64 bits
#ifdef DEBUG
    // Debug str only works for constexpr strings since we cant own the string data
    const char* DEBUG_STR = DEBUG_STR_EMPTY;
#endif
    friend struct std::hash<StrToken>;

    void initFromStrInternal(const char* str, size_t sz);

};
#ifdef DEBUG
static_assert(sizeof(StrToken) == 32);
#else
static_assert(sizeof(StrToken) == 24);
#endif

namespace std {
    template <>
    struct hash<StrToken> {
        auto operator()(const StrToken& token) const -> size_t {
            size_t seed = 0;
            boost::hash_combine(seed, std::hash<ui64>()(token.mTokenLow));
            boost::hash_combine(seed, std::hash<ui64>()(token.mTokenMid));
            boost::hash_combine(seed, std::hash<ui64>()(token.mTokenHigh));
            return seed;
        }
    };
}

// Guarenteed consteval initialization
template<size_t N>
inline consteval StrToken CStrToken(const char(&str)[N]) {
    return StrToken(str, false /*Allows us to select the consteval constructor*/);
}

template<>
struct std::formatter<StrToken> : std::formatter<std::string> {
    template<typename FormatContext>
    auto format(const StrToken& str, FormatContext& ctx) const -> decltype(ctx.out()) {
        // Directly format the string representation of StrToken
        return std::formatter<std::string>::format(str.toString(), ctx);
    }
};