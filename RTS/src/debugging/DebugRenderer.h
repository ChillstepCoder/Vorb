#pragma once

constexpr int MAX_DEBUG_RENDER_LIFETIME = INT32_MAX;

class DebugRenderer
{
public:
    // =============== Main thread only ===============
    static void drawVector(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime = 0, int id = 0);
    static void drawVector(const f32v3& origin, const f32v3& vec, color4 color, int lifeTime = 0, int id = 0);
    static void drawLine(const f32v2& origin, const f32v2& vec, color4 color, int lifeTime = 0, int id = 0);
    static void drawLine(const f32v3& origin, const f32v3& vec, color4 color, int lifeTime = 0, int id = 0);
    static void drawLineBetweenPoints(const f32v2& origin, const f32v2& end, color4 color, int lifeTime = 0, int id = 0);
    static void drawLineBetweenPoints(const f32v3& origin, const f32v3& end, const color4& color, int lifeTime = 0, int id = 0);
    static void drawWireQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime = 0, int id = 0);
    static void drawWireQuad(const f32v3& origin, const f32v2& dims, color4 color, int lifeTime = 0, int id = 0);
    static void drawFilledQuad(const f32v2& origin, const f32v2& dims, color4 color, int lifeTime = 0, int id = 0);
    static void drawFilledQuad(const f32v3& origin, const f32v2& dims, color4 color, int lifeTime = 0, int id = 0);
    static void drawWireTriangle(const f32v3& v0, const f32v3& v1, const f32v3& v2, color4 color, int lifeTime = 0, int id = 0);
    static void reserveFilledQuads(ui32 count, int lifeTime = 0, int id = 0);
    static void reserveLines(ui32 count, int lifeTime = 0, int id = 0);
    static void drawAABB(const i32AABB2& aabb, f32 height, color4 color, int lifeTime = 0, int id = 0);
    static void drawAABB(const i32AABB3& aabb, color4 color, int lifeTime = 0, int id = 0);
    static void drawAABB(const f32AABB3& aabb, color4 color, int lifeTime = 0, int id = 0);
	static void drawAABB(const f32v2& botLeft, const f32v2& botRight, const f32v2& topLeft, const f32v2& topRight, f32 height, color4 color, int lifeTime = 0, int id = 0);
    static void drawAABB(const f32v2& botLeft, const f32v2& dims, f32 height, color4 color, int lifeTime = 0, int id = 0);
    static void drawPath(const std::vector<f32v3>& path, color4 color, int lifeTime = 0, int id = 0);
    static void drawCircle(const f32v3& origin, f32 radius, color4 color, int lifeTime = 0, int id = 0);

    // =============== Thread safe functions ===============
    static void drawFilledQuadThreadSafe(const f32v3& p1, const f32v3& p2, const f32v3& p3, const f32v3& p4, color4 color, int lifeTime = 0, int id = 0);
    static void drawLineBetweenPointsThreadSafe(const f32v3& origin, const f32v3& end, const color4& color, int lifeTime = 0, int id = 0);
    static void drawWireQuadThreadSafe(const f32v3& origin, const f32v2& dims, color4 color, int lifeTime = 0, int id = 0);
    static void drawAABBThreadSafe(const f32AABB3& aabb, color4 color, int lifeTime = 0, int id = 0);

	// TODO: static void drawText()
	static void render(const f32v3& cameraPos, const f32m4& viewMatrix);
	static void clearAllMeshesWithId(int id);
	static void clearAll();
};