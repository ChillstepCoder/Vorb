#pragma once

class SimpleSpriteComponent {
public:
	SimpleSpriteComponent(VGTexture texture, const f32v2& dims) : mTexture(texture), mDims(dims) { }

	VGTexture mTexture = 0;
	float mAngle = 0.0f;
	f32v2 mDims = f32v2(2.0f);
	f32v4 mUvs = f32v4(0.0f, 0.0f, 1.0f, 1.0f);
	color4 mColor = color4(1.0f, 1.0f, 1.0f);
	float mHitFlash = 0.0f;
};
