#pragma once

class MaterialShaderDef;

class TonemapRenderer
{
public:
    TonemapRenderer();
    ~TonemapRenderer();

    void render(VGTexture lightTextureInput);
private:
    AssetHandlePtr<MaterialShaderDef> mShaderDef;
};

