#pragma once

class BrushDef : public IAsset {
public:
    std::vector<ui8> data; // A8 alpha only
    ui32v2 dims;
    VGTexture texture; // TODO: Remove?
};