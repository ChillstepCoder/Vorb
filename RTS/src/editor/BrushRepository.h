#pragma once

DECL_VIO(class IOManager);

class TextureRepository;

#include <Vorb/graphics/BitmapResource.h>

class Brush {
public:
    std::vector<ui8> data;
    ui32v2 dims;
    VGTexture texture;
    nString name;
};

class BrushRepository {
public:
    BrushRepository(vio::IOManager& ioManager);
    ~BrushRepository();

    void loadBrush(const vio::Path& filePath, TextureRepository& textureRepository);
    const std::vector<Brush>& getBrushes() const { return mBrushes; }

private:
    vio::IOManager& mIomanager;
    std::vector<Brush> mBrushes;
};

