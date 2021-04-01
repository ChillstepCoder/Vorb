#pragma once
struct b2AABB;

class DebugRenderer
{
public:
	static void drawVector(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime = 0, int id = 0);
    static void drawLine(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime = 0, int id = 0);
    static void drawBox(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime = 0, int id = 0);
    static void drawQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime = 0, int id = 0);
	static void drawAABB(const b2AABB& aabb, color4 color, int lifeTime = 0, int id = 0);
	static void drawAABB(const f32v2& botLeft, const f32v2& botRight, const f32v2& topLeft, const f32v2& topRight, color4 color, int lifeTime = 0, int id = 0);
    static void drawAABB(const f32v2& botLeft, const f32v2& dims, color4 color, int lifeTime = 0, int id = 0);

	// TODO: static void drawText()
	static void render(const f32m4& viewMatrix);
	static void clearAllMeshesWithId(int id);
	static void clearAll();
};

