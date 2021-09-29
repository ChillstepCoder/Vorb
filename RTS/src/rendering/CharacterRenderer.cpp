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

    BillboardMesh mesh;
    const f32v3 cameraOffset2D(-camera.getDirection().x * 0.09f, -camera.getDirection().y * 0.09f, 0.0f);
    
    static const float SIZE = 1.4f;
    static const float HEAD_OFFSET_MULT_Y = 0.28f;
    static const float HEAD_OFFSET_MULT_X = 0.04f;
    const f32v2 headOffset = f32v2(SIZE * HEAD_OFFSET_MULT_X * headOffsetX, SIZE * HEAD_OFFSET_MULT_Y);
    const f32v2 bodyOffset = f32v2(0.0f, 0.0f * SIZE * 0.25f); // TODO: THIS IS DISABLED
    buildPart(mesh, position, bodyOffset, *model.mBodySprites[index], shouldFlip, SIZE, alpha);
    buildPart(mesh, position + cameraOffset2D, headOffset, *model.mFaceSprites[index], shouldFlip, SIZE, alpha);
    buildPart(mesh, position + cameraOffset2D * 2.0f, headOffset, *model.mHairSprites[index], shouldFlip, SIZE, alpha);

    // TODO: Store in component
    mesh.finishMesh(QuadMeshDrawMode::STREAM);
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

void CharacterRenderer::buildPart(BillboardMesh& mesh, const f32v3& rootPos, const f32v2& offset, const SpriteData& spriteData, bool shouldFlip, float width, float alpha) {
    mesh.addQuad(rootPos, f32v2(width, width), offset, spriteData.atlasPage, spriteData.uvs, color4(1.0f, 1.0f, 1.0f, alpha), shouldFlip, 0u);
}
