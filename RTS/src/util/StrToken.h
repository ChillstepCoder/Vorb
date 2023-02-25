#pragma once

constexpr ui64 strTokenEncodeChar(const char c) {
    if (c >= 'a' && c <= 'z') {
        return ui64(c) - 'a' + 1ull;
    }
    else if (c >= 'A' && c <= 'Z') {
        return ui64(c) - 'A' + 1ull;
    }
    return ui64(0); // Underscores, ect 
}

constexpr ui64 strTokenEncodeIndex(const char c) {
    if (c >= '0' && c <= '9') {
        return ui64(c) - '0';
    }
    return ui64(0);
}

constexpr ui64 TOKEN_INDEX_MASK = ui64(0xf) << 60ull;
constexpr int MAX_CHARS_IN_STRTOKEN = 12; // Does not include index
constexpr int MAX_CHARS_IN_STRTOKEN_WITH_INDEX = 13;

// Constexpr 64 bit compressed lower case 12 character string with optional integer at end
// For fast comparison and serialization
class StrToken
{
public:
    constexpr StrToken() : mToken(0u) {}
    constexpr StrToken(ui64 token) : mToken(token) {}

    template<size_t N>
    explicit constexpr StrToken(const char(&str)[N]) :
        mToken(
            ((N > 0 ? strTokenEncodeChar(str[0]) : 0ull))         |
            ((N > 1 ? strTokenEncodeChar(str[1]) : 0ull)  << 5)   |
            ((N > 2 ? strTokenEncodeChar(str[2]) : 0ull)  << 10)  |
            ((N > 3 ? strTokenEncodeChar(str[3]) : 0ull)  << 15)  |
            ((N > 4 ? strTokenEncodeChar(str[4]) : 0ull)  << 20)  |
            ((N > 5 ? strTokenEncodeChar(str[5]) : 0ull)  << 25)  |
            ((N > 6 ? strTokenEncodeChar(str[6]) : 0ull)  << 30)  |
            ((N > 7 ? strTokenEncodeChar(str[7]) : 0ull)  << 35)  |
            ((N > 8 ? strTokenEncodeChar(str[8]) : 0ull)  << 40)  |
            ((N > 9 ? strTokenEncodeChar(str[9]) : 0ull)  << 45)  |
            ((N > 10 ? strTokenEncodeChar(str[10]) : 0ull) << 50)  |
            ((N > 11 ? strTokenEncodeChar(str[11]) : 0ull) << 55)  |
            ((N > 12 ? strTokenEncodeIndex(str[12]) : 0ull) << 60)
        ) {}

    template<size_t N>
    explicit constexpr StrToken(const char(&str)[N], ui64 index) :
        mToken(
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
            ((N > 11 ? strTokenEncodeChar(str[11]) : 0ull) << 55) |
            (index << 60)
        ) {}

    StrToken(const nString& str);

    constexpr operator ui64() const { return mToken; }

    // Buffer length must be at least 14
    void toString(OUT char* outStr, OUT ui32* outLength) const;

    // Index can be 0-15
    ui32 getIndex() const { return (ui32)(mToken >> 60); }
    void setIndex(ui32 index) { assert(index <= 0xfu); mToken = (mToken & (~TOKEN_INDEX_MASK)) | ((ui64)index << 60ull); }

    ui64 mToken;
};
static_assert(sizeof(StrToken) == 8);

namespace std {
    template <>
    struct hash<StrToken> {
        auto operator()(const StrToken& token) const -> size_t {
            return hash<ui64>{}(token.mToken);
        }
    };
}  // namespace std