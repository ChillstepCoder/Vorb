#pragma once
// Static class for the PBR BRDF texture
class BrdfLUT
{
public:
    BrdfLUT() = delete;

    static void loadOrComputeTexture();
    static VGTexture getTexture() { return sTexture; }
private:
    static VGTexture sTexture;
};

