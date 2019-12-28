#pragma once
#include "stdafx.h"

struct SimpleSpriteComponent {
	// TODO: Compress
	vecs::ComponentID physicsComponent = 0;
	VGTexture texture = 0;
	float angle = 0.0f;
	f32v2 dims = f32v2(2.0f);
	f32v4 uvs = f32v4(0.0f, 0.0f, 1.0f, 1.0f);
	color4 color = color4(1.0f, 1.0f, 1.0f);
};

class SimpleSpriteComponentTable : public vecs::ComponentTable<SimpleSpriteComponent> {
public:
	static const std::string& NAME;
};