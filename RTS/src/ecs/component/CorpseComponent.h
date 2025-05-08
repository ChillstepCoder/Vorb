#pragma once

struct CorpseComponent {
	float mAge = 0.0f;
	float mBlood = 0.0f;
};

class CorpseComponentTable {
public:
	void update();
};