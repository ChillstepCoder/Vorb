#pragma once
// T should be an unsigned enum or enum class where each flag is a bit (not a sequential integer)
// TODO: Constexpr?
template<typename T>
class BitFlags
{
    static_assert(std::is_unsigned<typename std::underlying_type<T>::type>()); // Bits are unsigned
public:
    BitFlags() {};
    BitFlags(typename std::underlying_type<T>::type startBits) : mBits(startBits) {};
    BitFlags(T b1) : mBits(e_cast(b1)) {};
    BitFlags(T b1, T b2) : mBits(e_cast(b1) | e_cast(b2)) {};
    BitFlags(T b1, T b2, T b3) : mBits(e_cast(b1) | e_cast(b2) | e_cast(b3)) {};
    BitFlags(T b1, T b2, T b3, T b4) : mBits(e_cast(b1) | e_cast(b2) | e_cast(b3) | e_cast(b4)) {};

    BitFlags<T>& operator|=(const BitFlags<T>& other) {
        mBits |= other.mBits;
        return *this;
    }

    BitFlags<T>& operator|=(const T other) {
        mBits |= e_cast(other);
        return *this;
    }

    // ============== Mutators ==============

    void setBit(T bit) { mBits |= e_cast(bit); }
    void overwriteBits(T bits) { mBits = e_cast(bits); }

    template <typename... T>
    void setBits(T... args) {
        // Binary fold, wheeeeee
        mBits = (mBits | ... | e_cast(args));
    }
    void clearBit(T bit) { mBits &= (~e_cast(bit)); }
    // Clears all bits
    void clearBits() { mBits = {}; }
    // Clears a specific set of bits
    void clearMaskBits(typename std::underlying_type<T>::type mask) { mBits &= (~mask); }

    // ============== Accessors ==============

    typename std::underlying_type<T>::type getBits() const { return mBits; }
    bool isBitSet(T bit) const { return (mBits & e_cast(bit)) != 0; }

    // Return true if all bits in the mask are set
    bool isMaskSet(typename std::underlying_type<T>::type mask) const { return (mBits & mask) == mask; }
    // Return true if any bits in the mask are set
    bool isMaskPartiallySet(typename std::underlying_type<T>::type mask) const { return (mBits & mask) != 0; }

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

