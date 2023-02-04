#pragma once

#include "rendering/mesh/Mesh.h"

#include "rendering/font/Font.h"

struct Font;

// TBO Billboards
struct GlyphUniformData {
    f32v4 uvRect;
    TextureHandle textureDiffuse;
};

// TODO: Unicode
class TextMeshBuilder {
public:
    TextMeshBuilder();
    ~TextMeshBuilder();

    void addString(const nString& str, const f32v3& rootPosition, const Font& font, f32 glyphHeight, const f32v2& offset2D, TextAlign align, const f32v4 clipRect = f32v4(-1000.0f, -1000.0f, 2000.0f, 2000.0f), bool shouldWrap = true);

    void finishMesh(Mesh& mesh, MeshDrawMode drawMode);
private:

    struct GlyphData {
        f32v4 uvRect;
        f32v3 mOrigin;
        f32v2 mDims;
        f32v2 xyOffset;
    };
    static_assert(sizeof(GlyphData) == 44);

    struct FontMeshData {
        void clear() {
            mGlyphs.clear();
        }

        std::vector<GlyphData> mGlyphs;
    };

    void initMeshBuffers(MeshGpuData& subMesh);
    void uploadBufferData(MeshGpuData& subMesh, const FontMeshData& data, MeshDrawMode drawMode);

    // Map font texture IDs to fonts 
    FontMeshData                       mFontData;
    BoundingSphere                     mBoundingSphere;
};

