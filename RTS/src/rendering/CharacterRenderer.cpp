#include "CharacterRenderer.h"

void renderPart(vg::SpriteBatch& sb, const vg::Texture& body, const f32v2& pos, const f32v2& offset, const f32v2& additionalOffset, f32v4& uvRect, float size, float depth) {
	f32v2 sizeVec(size);
	f32v2 newPos = pos + offset - sizeVec.x * 0.5f;
	sb.draw(body.id, &uvRect, nullptr, newPos, -additionalOffset, sizeVec, 0.0f /*rotation*/, color4(1.0f, 1.0f, 1.0f), depth);
}

void CharacterRenderer::render(vg::SpriteBatch& sb, const CharacterModel& model, const f32v2& pos, float angle) {

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

	std::cout << angle << " " << index << std::endl;

	static const float BODY_SIZE = 2.0f;
	static const float HEAD_SIZE = 2.0f;
	static const float HEAD_OFFSET_MULT_Y = 0.125f;
	static const float HEAD_OFFSET_MULT_X = 0.025f;
	const f32v2 headOffset = f32v2(HEAD_SIZE * HEAD_OFFSET_MULT_X * headOffsetX, BODY_SIZE * HEAD_OFFSET_MULT_Y);
	const f32v2 bodyOffset = f32v2(0.0f, BODY_SIZE * 0.25f);
	renderPart(sb, model.mBodyTextures[index], pos, bodyOffset, f32v2(0.0f), uvRect, BODY_SIZE, 0.0f);
	renderPart(sb, model.mFaceTextures[index], pos, bodyOffset, headOffset, uvRect, HEAD_SIZE, 0.1f);
	renderPart(sb, model.mHairTextures[index], pos, bodyOffset, headOffset, uvRect, HEAD_SIZE, 0.2f);
}
