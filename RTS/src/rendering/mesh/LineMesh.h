#pragma once

struct LineVertex {
    f32v3 pos;
    color4 color;
};

class LineMesh
{
public:
    LineMesh();
    ~LineMesh();

    void initialize(const std::vector<LineVertex>& vertices);

    bool isValid() const { return mVao != 0; }

    // Call before drawing
    void bind();

    void drawLineStrip(int start = 0, ui32 count = 0) const;
    void drawPoints(int start = 0, ui32 count = 0) const;
    void drawLines(int start = 0, ui32 count = 0) const;

    VGBuffer mVao = 0;
    VGBuffer mVbo = 0;
    ui32 mNumVertices = 0;
};

namespace LineMeshBuilders {
    inline void addLineBetweenPoints(std::vector<LineVertex>& vertices, f32v3 p1, f32v3 p2, color4 color) {
        vertices.reserve(vertices.size() + 2);
        vertices.emplace_back(p1, color);
        vertices.emplace_back(p2, color);
    }
    inline void addAABBLines(std::vector<LineVertex>& vertices, const f32AABB3& aabb, color4 color) {
        vertices.reserve(vertices.size() + 12);

        // Lines from the minimum corner
        f32v3 minX = aabb.pos;
        f32v3 maxX = aabb.pos + f32v3(aabb.dims.x, 0, 0);
        f32v3 maxY = aabb.pos + f32v3(0, aabb.dims.y, 0);
        f32v3 maxZ = aabb.pos + f32v3(0, 0, aabb.dims.z);

        addLineBetweenPoints(vertices, minX, maxX, color);
        addLineBetweenPoints(vertices, minX, maxY, color);
        addLineBetweenPoints(vertices, minX, maxZ, color);

        addLineBetweenPoints(vertices, maxX, maxX + f32v3(0, aabb.dims.y, 0), color);
        addLineBetweenPoints(vertices, maxZ, maxZ + f32v3(aabb.dims.x, 0, 0), color);
        addLineBetweenPoints(vertices, maxY, maxY + f32v3(0, 0, aabb.dims.z), color);

        addLineBetweenPoints(vertices, maxX, maxX + f32v3(0, 0, aabb.dims.z), color);
        addLineBetweenPoints(vertices, maxZ, maxZ + f32v3(0, aabb.dims.y, 0), color);
        addLineBetweenPoints(vertices, maxY, maxY + f32v3(aabb.dims.x, 0, 0), color);

        f32v3 maxXYZ = aabb.pos + aabb.dims;
        addLineBetweenPoints(vertices, maxXYZ, maxXYZ - f32v3(aabb.dims.x, 0, 0), color);
        addLineBetweenPoints(vertices, maxXYZ, maxXYZ - f32v3(0, aabb.dims.y, 0), color);
        addLineBetweenPoints(vertices, maxXYZ, maxXYZ - f32v3(0, 0, aabb.dims.z), color);
    }

}