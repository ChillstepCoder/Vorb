#pragma once

struct WorldTextRenderState;
struct Font;
class Camera3D;
class Mesh;
class MaterialShaderDef;

class WorldTextRenderer {
public:
    WorldTextRenderer();
    ~WorldTextRenderer();

    void renderWorldText(const std::vector<WorldTextRenderState>& texts, const Camera3D& camera);

private:
    const Font* mFont;
    std::unique_ptr<Mesh> mTextMesh;
    AssetHandleBasePtr mShaderHandle;
    const MaterialShaderDef* mShader = nullptr;
};

