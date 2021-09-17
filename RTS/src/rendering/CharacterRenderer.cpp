#include "stdafx.h"
#include "CharacterRenderer.h"

#include "rendering/MaterialManager.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/TileVertex.h"
#include "rendering/SpriteData.h"

#include "Random.h"

#include "QuadMesh.h"
#include "camera/Camera3D.h"

//void renderPart(vg::SpriteBatch& sb, const vg::Texture& body, const f32v2& pos, f32 zPos, const f32v2& offset, const f32v2& additionalOffset, f32v4& uvRect, float size, float depth, float alpha) {
//	//f32v2 sizeVec(size);
//	//f32v2 newPos = pos + offset - sizeVec.x * 0.5f;
//	//sb.draw(body.id, &uvRect, nullptr, newPos, -additionalOffset, sizeVec, 0.0f /*rotation*/, color4(1.0f, 1.0f, 1.0f, alpha), depth + zPos);
//
//    BillboardVertex verts[4];
//}

CharacterRenderer::CharacterRenderer(const MaterialManager& materialManager) :
    mMaterial(materialManager.getMaterial("billboard"))
{

}

void CharacterRenderer::render(const Camera3D& camera, const MaterialRenderer& materialRenderer, const CharacterModel& model, const f32v3& position, float angle, float alpha) {

    int index = CHARACTER_MODEL_TEXTURE_FRONT;
    angle = RAD_TO_DEG(angle);
    float headOffsetX = 0.0f;
    bool shouldFlip = false;
    if (angle > 45.0f && angle < 135.0f) {
        index = CHARACTER_MODEL_TEXTURE_BACK;
    }
    else if (angle <= 45.0f && angle >= -45) {
        // Right
        headOffsetX = 1.0f;
        index = CHARACTER_MODEL_TEXTURE_SIDE;
    }
    else if (angle <= -135.0f || angle >= 135) {
        // Left
        headOffsetX = -1.0f;
        index = CHARACTER_MODEL_TEXTURE_SIDE;
        shouldFlip = true;
    }

    //std::cout << angle << " " << index << std::endl;

    std::vector<BillboardVertex> verts;
    const f32v3 cameraOffset2D(-camera.getDirection().x * 0.09f, -camera.getDirection().y * 0.09f, 0.0f);
    
    static const float SIZE = 1.4f;
    static const float HEAD_OFFSET_MULT_Y = 0.28f;
    static const float HEAD_OFFSET_MULT_X = 0.04f;
    const f32v2 headOffset = f32v2(SIZE * HEAD_OFFSET_MULT_X * headOffsetX, SIZE * HEAD_OFFSET_MULT_Y);
    const f32v2 bodyOffset = f32v2(0.0f, 0.0f * SIZE * 0.25f); // TODO: THIS IS DISABLED
    buildPart(verts, position, bodyOffset, *model.mBodySprites[index], shouldFlip, SIZE, 0.0f, alpha);
    buildPart(verts, position + cameraOffset2D, headOffset, *model.mFaceSprites[index], shouldFlip, SIZE, 0.0f, alpha);
    buildPart(verts, position + cameraOffset2D * 2.0f, headOffset, *model.mHairSprites[index], shouldFlip, SIZE, 0.0f, alpha);

    // TODO: Store in component
    BillboardMesh mesh;
    mesh.setData(verts.data(), verts.size(), QuadMeshDrawMode::STREAM);
    materialRenderer.renderMesh(mesh, *mMaterial);
    // Render shadow part
    // TODO: move over to decal system
    /*constexpr float MIN_SHADOW_ALPHA = 0.2f;
    constexpr float MAX_SHADOW_ALPHA = 0.6f;
    float shadowAlpha = vmath::clamp(MAX_SHADOW_ALPHA - zPos * 0.25f, MIN_SHADOW_ALPHA, MAX_SHADOW_ALPHA);
    renderPart(position, sShadowTexture, xyPos, 0.0f, f32v2(0.0f), f32v2(0.0f), uvRect, BODY_SIZE * 0.5f, 0.01f, alpha < 0.99f ? 0.0f : shadowAlpha);*/
}

// Prevent rounding errors, 0.0001 is half a pixel
constexpr f32 UV_EPSILON = 0.0001f;
constexpr f32 UV_EPSILON_2 = 2.0f * UV_EPSILON;

void CharacterRenderer::buildPart(std::vector<BillboardVertex>& vertexData, const f32v3& rootPos, const f32v2& offset, const SpriteData& spriteData, bool shouldFlip, float width, float depth, float alpha) {
    vertexData.resize(vertexData.size() + 4);

    BillboardVertex* verts = &vertexData.back() - 3;

    //// Center the sprite
    //// TODO: This shouldn't be hard coded to xy
    //const f32v2 offset(-(float)((spriteData.dimsMeters.x - 1) / 2) + spriteData.offset.x, spriteData.offset.y);
    //tilePosition.x += 0.5f;
    //tilePosition.y += 0.5f;

    f32v4 adjustedUvs;
    const f32v4& uvs = spriteData.uvs;
    if (shouldFlip) {
        // Flip horizontal
        adjustedUvs.x = uvs.x + uvs.z - UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = -uvs.z + UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }
    else {
        adjustedUvs.x = uvs.x + UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = uvs.z - UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }

    color4 topColor = color4((ui8)255u, (ui8)255u, (ui8)255u, (ui8)(alpha * 255));
    color4 bottomColor = topColor;
    const f32 halfX = width * 0.5f;

    { // Bottom Left
        BillboardVertex& vbl = verts[0];
        vbl.rootPos.x = rootPos.x;
        vbl.rootPos.y = rootPos.y;
        vbl.rootPos.z = rootPos.z;
        vbl.uvs.x = adjustedUvs.x;
        vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbl.color = bottomColor;
        vbl.atlasPage = spriteData.atlasPage;
        vbl.xzOffset.x = offset.x - halfX;
        vbl.xzOffset.y = offset.y;
    }
    { // Bottom Right
        BillboardVertex& vbr = verts[1];
        vbr.rootPos.x = rootPos.x;
        vbr.rootPos.y = rootPos.y;
        vbr.rootPos.z = rootPos.z;
        vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbr.color = bottomColor;
        vbr.atlasPage = spriteData.atlasPage;
        vbr.xzOffset.x = offset.x + halfX;
        vbr.xzOffset.y = offset.y;
    }

    { // Top Left
        BillboardVertex& vtl = verts[2];
        vtl.rootPos.x = rootPos.x;
        vtl.rootPos.y = rootPos.y;
        vtl.rootPos.z = rootPos.z;
        vtl.uvs.x = adjustedUvs.x;
        vtl.uvs.y = adjustedUvs.y;
        vtl.color = topColor;
        vtl.atlasPage = spriteData.atlasPage;
        vtl.xzOffset.x = offset.x - halfX;
        vtl.xzOffset.y = offset.y + width;
        vtl.windInfluence = 0;
    }
    { // Top Right
        BillboardVertex& vtr = verts[3];
        vtr.rootPos.x = rootPos.x;
        vtr.rootPos.y = rootPos.y;
        vtr.rootPos.z = rootPos.z;
        vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vtr.uvs.y = adjustedUvs.y;
        vtr.color = topColor;
        vtr.atlasPage = spriteData.atlasPage;
        vtr.xzOffset.x = offset.x + halfX;
        vtr.xzOffset.y = offset.y + width;
        vtr.windInfluence = 0;
    }
}
