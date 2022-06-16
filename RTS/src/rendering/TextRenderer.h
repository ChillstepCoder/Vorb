#pragma once

#include "rendering/font/Font.h"
#include "rendering/mesh/Mesh.h"

class Material;
class MaterialRenderer;

struct TextMeshBillboard {
    friend class TextRenderer;
public:
    TextMeshBillboard();
    virtual ~TextMeshBillboard();

    VORB_NON_COPYABLE_BUT_MOVABLE(TextMeshBillboard);

private:
    Mesh mMesh;
    f32v3 mRootPosition;
};

// TODO: Unicode?
class TextRenderer
{
public:
    TextRenderer(const MaterialRenderer& materialRenderer);

    void createTextMesh(Font& font, TextMeshBillboard& mesh, const cString text, const f32v3& rootPosWorld, const f32v2& offset2D, const f32 glyphHeight, const color4& color, TextAlign align = TextAlign::CENTER, const f32v4& clipRect = f32v4(-1000.0f, -1000.0f, 1000.0f, 1000.0f), bool shouldWrap = false);

private:
    const MaterialRenderer& mMaterialRenderer;
    const Material* mBillboardMaterial = nullptr;
};

