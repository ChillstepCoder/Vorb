#pragma once
class BitArray
{
public:
    BitArray();
    ~BitArray();

    void resize(ui32 size);
    void resizeAndZero(ui32 size);
    void setBitTo(ui32 index, bool val);
    bool getBit(ui32 index);
    void zeroAllBits();

private:
    std::vector<ui8> mData;
};

