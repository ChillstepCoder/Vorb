#pragma once
class BitArray
{
public:
    BitArray();
    ~BitArray();

    void resize(ui32 size);
    void resizeAndZero(ui32 size);
    void setBitTo(ui32 index, bool val);
    bool getBit(ui32 index) const;
    void zeroAllBits();

    void debugPrint(int width, int height) const;
private:
    std::vector<ui8> mData;
};

