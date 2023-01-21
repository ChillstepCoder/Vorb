#pragma once

class MaterialShader;

class TonemapRenderer
{
public:
    TonemapRenderer();
    ~TonemapRenderer();

    void render(VGTexture lightTextureInput);
private:
    const MaterialShader* mShader = nullptr;
};

