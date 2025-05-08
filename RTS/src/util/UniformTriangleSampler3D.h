#pragma once

// Based on https://blogs.sas.com/content/iml/2020/10/19/random-points-in-triangle.html but extended to 3D
class UniformTriangleSampler3D {
public:
    UniformTriangleSampler3D() {}
    UniformTriangleSampler3D(const f32v3 p1, const f32v3 p2, const f32v3 p3)
        : P1(p1) {
        a = p2 - P1;
        b = p3 - P1;
    }
    void init(const f32v3 p1, const f32v3 p2, const f32v3 p3) {
        P1 = p1;
        a = p2 - P1;
        b = p3 - P1;
    }

    static f32v2 getRandomUV() {
        f32v2 uv(Random::getCachedRandomf(), Random::getCachedRandomf());

        // Reflect the point outside the triangle
        if (uv.x + uv.y > 1.0f) {
            uv.x = 1.0f - uv.x;
            uv.y = 1.0f - uv.y;
        }

        return uv;
    }

    f32v3 generatePoint() const {
        return getPoint(getRandomUV());
    }

    f32v3 getPoint(f32v2 uv) const {
        return P1 + uv.x * a + uv.y * b;
    }

    std::vector<f32v3> generatePoints(int n) const {
        std::vector<f32v3> points;
        points.reserve(n);
        for (int i = 0; i < n; ++i) {
            points.push_back(generatePoint());
        }
        return points;
    }

    f32 getArea() const {
        return glm::length(glm::cross(a, b)) / 2.0f;
    }

    // Helper function to get barycentric coordinates
    f32v3 getBarycentricCoords(const f32v3& point) const {
        f32v3 v0 = b, v1 = a, v2 = point - P1;
        float d00 = glm::dot(b, v0);
        float d01 = glm::dot(v0, v1);
        float d11 = glm::dot(v1, v1);
        float d20 = glm::dot(v2, v0);
        float d21 = glm::dot(v2, v1);
        float denom = d00 * d11 - d01 * d01;
        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        float u = 1.0f - v - w;
        return f32v3(u, v, w);
    }

private:
    f32v3 P1;  // Triangle vertices in 3D
    f32v3 a, b;        // Vectors defining triangle sides
};
