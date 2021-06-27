#include "stdafx.h"
#include "CharacterRenderer.h"

vg::Texture sShadowTexture;

void renderPart(vg::SpriteBatch& sb, const vg::Texture& body, const f32v2& pos, f32 zPos, const f32v2& offset, const f32v2& additionalOffset, f32v4& uvRect, float size, float depth, float alpha) {
	f32v2 sizeVec(size);
	f32v2 newPos = pos + offset - sizeVec.x * 0.5f;
	newPos.y += zPos * Z_TO_XY_RATIO;
	sb.draw(body.id, &uvRect, nullptr, newPos, -additionalOffset, sizeVec, 0.0f /*rotation*/, color4(1.0f, 1.0f, 1.0f, alpha), depth + zPos);
}

void CharacterRenderer::render(vg::SpriteBatch& sb, const CharacterModel& model, const f32v2& xyPos, f32 zPos, f32 depthOffset, float angle, float alpha) {

	f32v4 uvRect(0.0f, 1.0f, 1.0f, -1.0f);
	int index = CHARACTER_MODEL_TEXTURE_FRONT;
	angle = RAD_TO_DEG(angle);
	float headOffsetX = 0.0f;
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
		uvRect.x = 1.0f;
		uvRect.z = -1.0f;
		headOffsetX = -1.0f;
		index = CHARACTER_MODEL_TEXTURE_SIDE;
	}

	//std::cout << angle << " " << index << std::endl;

	static const float BODY_SIZE = 1.4f;
	static const float HEAD_SIZE = 1.4f;
	static const float HEAD_OFFSET_MULT_Y = 0.125f;
	static const float HEAD_OFFSET_MULT_X = 0.025f;
	const f32v2 headOffset = f32v2(HEAD_SIZE * HEAD_OFFSET_MULT_X * headOffsetX, BODY_SIZE * HEAD_OFFSET_MULT_Y);
	const f32v2 bodyOffset = f32v2(0.0f, BODY_SIZE * 0.25f);
	renderPart(sb, model.mBodyTextures[index], xyPos, zPos, bodyOffset, f32v2(0.0f), uvRect, BODY_SIZE, 0.3f + depthOffset, alpha);
	renderPart(sb, model.mFaceTextures[index], xyPos, zPos, bodyOffset, headOffset, uvRect, HEAD_SIZE, 0.6f + depthOffset, alpha);
	renderPart(sb, model.mHairTextures[index], xyPos, zPos, bodyOffset, headOffset, uvRect, HEAD_SIZE, 0.8f + depthOffset, alpha);

	// Render shadow part
    // TODO: move over to decal system
    constexpr float MIN_SHADOW_ALPHA = 0.2f;
    constexpr float MAX_SHADOW_ALPHA = 0.6f;
    float shadowAlpha = vmath::clamp(MAX_SHADOW_ALPHA - zPos * 0.25f, MIN_SHADOW_ALPHA, MAX_SHADOW_ALPHA);
    renderPart(sb, sShadowTexture, xyPos, 0.0f, f32v2(0.0f), f32v2(0.0f), uvRect, BODY_SIZE * 0.5f, 0.01f, alpha < 0.99f ? 0.0f : shadowAlpha);
}
