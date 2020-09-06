#pragma once
struct b2AABB;

class DebugRenderer
{
public:
	static void drawVector(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime = 0);
    static void drawLine(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime = 0);
    static void drawBox(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime = 0);
	static void drawAABB(const b2AABB& aabb, color4 color, int lifeTime = 0);
	static void drawAABB(const f32v2& botLeft, const f32v2& botRight, const f32v2& topLeft, const f32v2& topRight, color4 color, int lifeTime = 0);

	static void renderLines(const f32m4& viewMatrix);
};

