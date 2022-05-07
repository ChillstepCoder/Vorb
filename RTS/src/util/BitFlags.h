#pragma once
// T should be an unsigned enum or enum class where each flag is a bit (not a sequential integer)
template<typename T>
class BitFlags
{
    static_assert(std::is_unsigned<typename std::underlying_type<T>::type>()); // Bits are unsigned
public:
    BitFlags() {};
    BitFlags(T startBit) : mBits(e_cast(startBit)) {};
    BitFlags(typename std::underlying_type<T>::type startBits) : mBits(startBits) {};

    // ============== Mutators ==============

    void setBit(T bit) { mBits |= e_cast(bit); }
    void overwriteBits(T bits) { mBits = bits; }

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

