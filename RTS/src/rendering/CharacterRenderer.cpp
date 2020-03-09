#include "CharacterRenderer.h"

void renderPart(vg::SpriteBatch& sb, const vg::Texture& body, const f32v2& pos, const f32v2& offset, f32v4& uvRect, float size, float depth) {
	f32v2 sizeVec(size);
	f32v2 newPos = pos + offset - sizeVec.x * 0.5f;
	sb.draw(body.id, &uvRect, newPos, sizeVec, color4(1.0f, 1.0f, 1.0f), depth);
}

void CharacterRenderer::render(vg::SpriteBatch& sb, const CharacterModel& model, const f32v2& pos, float angle) {

	f32v4 uvRect(0.0f, 1.0f, 1.0f, -1.0f);
	int index = CHARACTER_MODEL_TEXTURE_FRONT;
	angle = RAD_TO_DEG(angle);
	float headOffset = 0.0f;
	if (angle > 45.0f && angle < 135.0f) {
		index = CHARACTER_MODEL_TEXTURE_BACK;
	}
	else if (angle <= 45.0f && angle >= -45) {
		// Right
		headOffset = 1.0f;
		index = CHARACTER_MODEL_TEXTURE_SIDE;
	}
	else if (angle <= -135.0f || angle >= 135) {
		// Left
		uvRect.x = 1.0f;
		uvRect.z = -1.0f;
		headOffset = -1.0f;
		index = CHARACTER_MODEL_TEXTURE_SIDE;
	}

	std::cout << angle << " " << index << std::endl;

	static const float BODY_SIZE = 2.0f;
	static const float HEAD_SIZE = 2.0f;
	static const float HEAD_OFFSET_MULT_Y = 0.5f;
	static const float HEAD_OFFSET_MULT_X = 0.05f;
	renderPart(sb, model.mBodyTextures[index], pos, f32v2(0.0f, BODY_SIZE * 0.25f), uvRect, BODY_SIZE, 0.0f);
	const f32v2 headPos = f32v2(HEAD_SIZE * HEAD_OFFSET_MULT_X * headOffset, BODY_SIZE * HEAD_OFFSET_MULT_Y);
	renderPart(sb, model.mFaceTextures[index], pos, headPos, uvRect, HEAD_SIZE, 0.1f);
	renderPart(sb, model.mHairTextures[index], pos, headPos, uvRect, HEAD_SIZE, 0.2f);
}
