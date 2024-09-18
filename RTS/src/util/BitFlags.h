#pragma once
// T should be an unsigned enum or enum class where each flag is a bit (not a sequential integer)
// TODO: Constexpr?

template<typename T>
concept HasUnsignedUnderlyingType = requires {
    typename std::underlying_type_t<T>;
} && std::unsigned_integral<std::underlying_type_t<T>>;

template<HasUnsignedUnderlyingType T>
class BitFlags {
public:
    BitFlags() noexcept = default;
    BitFlags(typename std::underlying_type<T>::type startBits) noexcept : mBits(startBits) {};
    BitFlags(T b1) noexcept : mBits(e_cast(b1)) {};
    BitFlags(T b1, T b2) noexcept : mBits(e_cast(b1) | e_cast(b2)) {};
    BitFlags(T b1, T b2, T b3) noexcept : mBits(e_cast(b1) | e_cast(b2) | e_cast(b3)) {};
    BitFlags(T b1, T b2, T b3, T b4) noexcept : mBits(e_cast(b1) | e_cast(b2) | e_cast(b3) | e_cast(b4)) {};

    BitFlags<T>& operator|=(const BitFlags<T>& other) noexcept {
        mBits |= other.mBits;
        return *this;
    }

    BitFlags<T>& operator|=(const T other) noexcept {
        mBits |= e_cast(other);
        return *this;
    }

    BitFlags<T>& operator&=(const BitFlags<T>& other) noexcept {
        mBits &= other.mBits;
        return *this;
    }

    BitFlags<T>& operator&=(const T other) noexcept {
        mBits &= e_cast(other);
        return *this;
    }

    BitFlags<T> operator|(const BitFlags<T>& other) const noexcept {
        return BitFlags<T>(mBits | other.mBits);
    }

    BitFlags<T> operator&(const BitFlags<T>& other) const noexcept {
        return BitFlags<T>(mBits & other.mBits);
    }

    auto operator<=>(const BitFlags<T>&) const = default;

    operator typename std::underlying_type<T>::type& () noexcept {
        return mBits;
    }

    typename std::underlying_type<T>::type& getBits() noexcept {
        return mBits;
    }

    // ============== Mutators ==============

    void setBit(T bit) noexcept { mBits |= e_cast(bit); }
    void overwriteBits(T bits) noexcept { mBits = e_cast(bits); }

    template <typename... T>
    void setBits(T... args) noexcept {
        // Binary fold, wheeeeee
        mBits = (mBits | ... | e_cast(args));
    }
    void clearBit(T bit) noexcept { mBits &= (~e_cast(bit)); }
    // Clears all bits
    void clearBits() noexcept { mBits = {}; }
    // Clears a specific set of bits
    void clearMaskBits(typename std::underlying_type<T>::type mask) noexcept { mBits &= (~mask); }

    // ============== Accessors ==============

    typename std::underlying_type<T>::type& getBitsRef() noexcept { return mBits; }
    typename std::underlying_type<T>::type getBits() const noexcept { return mBits; }
    bool isBitSet(T bit) const noexcept { return (mBits & e_cast(bit)) != 0; }
    bool isBitUnset(T bit) const noexcept { return (mBits & e_cast(bit)) == 0; }

    // Return true if all bits in the mask are set
    bool isMaskSet(typename std::underlying_type<T>::type mask) const noexcept { return (mBits & mask) == mask; }
    // Return true if any bits in the mask are set
    bool isMaskPartiallySet(typename std::underlying_type<T>::type mask) const noexcept { return (mBits & mask) != 0; }

    // ============== Debugging ==============

    void debugPrintBits(const char* msg) {
        assert(msg);
        printf("%s", msg);
        for (int i = 0; i < sizeof(T) * 8; ++i) {
            printf(" %d", (int)isBitSet(T(1 << i)));
        }
        printf("\n");
    }


private:
    typename std::underlying_type<T>::type mBits{};
};

