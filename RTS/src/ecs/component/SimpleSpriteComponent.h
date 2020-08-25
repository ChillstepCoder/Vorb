#pragma once
#include <Vorb/ecs/Entity.h>
#include <Vorb/ecs/ComponentTable.hpp>

class SimpleSpriteComponent {
public:
	void init (vecs::ComponentID componentId, VGTexture texture, const f32v2& dims) {
		this->physicsComponent = componentId;
		this->texture = texture;
		this->dims = dims;
	}
	vecs::ComponentID physicsComponent = 0;
	VGTexture texture = 0;
	float angle = 0.0f;
	f32v2 dims = f32v2(2.0f);
	f32v4 uvs = f32v4(0.0f, 0.0f, 1.0f, 1.0f);
	color4 color = color4(1.0f, 1.0f, 1.0f);
	float hitFlash = 0.0f;
};

class SimpleSpriteComponentTable : public vecs::ComponentTable<SimpleSpriteComponent> {
public:
	static const std::string& NAME;

	void update();
};