#pragma once

#include "rendering/texture/SubTexture.h"
#include "rendering/font/Font.h"

class FontRepository
{
public:
    bool loadFont(const vio::Path& fontPath);
    const Font& getFont(const cString fontName);

private:
    static std::vector<ui32>* createRows(const std::vector<i32v4>& rects, ui32 r, ui32 padding, ui32& w);

    std::map<nString, Font> mFonts;
};

