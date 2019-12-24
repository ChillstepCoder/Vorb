#pragma once
#include "stdafx.h"


class DebugRenderer
{
public:
	static void drawVector(const f32v2& origin, const f32v2& vec, color4 color);

	static void renderLines(const f32m4& viewMatrix);
};

