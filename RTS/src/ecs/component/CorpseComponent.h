#pragma once
#include <Vorb/ecs/ComponentTable.hpp>

struct CorpseComponent {
	float mAge = 0.0f;
	float mBlood = 0.0f;
};

class CorpseComponentTable : public vecs::ComponentTable<CorpseComponent> {
public:
	static const std::string& NAME;

	void update();
};