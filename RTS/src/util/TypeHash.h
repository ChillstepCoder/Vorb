#pragma once

#include <boost/container_hash/hash.hpp>

namespace glm {
    template<typename T, glm::precision P>
    inline size_t hash_value(const glm::vec<2, T, P>& v) {
        size_t seed = 0;
        boost::hash_combine(seed, v.x);
        boost::hash_combine(seed, v.y);
        return seed;
    }

    template<typename T, glm::precision P>
    inline size_t hash_value(const glm::vec<3, T, P>& v) {
        size_t seed = 0;
        boost::hash_combine(seed, v.x);
        boost::hash_combine(seed, v.y);
        boost::hash_combine(seed, v.z);
        return seed;
    }

    inline size_t hash_value(const f32v2& v) { return hash_value<f32, glm::defaultp>(v); }
    inline size_t hash_value(const f32v3& v) { return hash_value<f32, glm::defaultp>(v); }
    inline size_t hash_value(const i32v2& v) { return hash_value<i32, glm::defaultp>(v); }
    inline size_t hash_value(const i32v3& v) { return hash_value<i32, glm::defaultp>(v); }
    inline size_t hash_value(const ui32v2& v) { return hash_value<ui32, glm::defaultp>(v); }
    inline size_t hash_value(const ui32v3& v) { return hash_value<ui32, glm::defaultp>(v); }
    inline size_t hash_value(const i16v2& v) { return hash_value<i16, glm::defaultp>(v); }
    inline size_t hash_value(const i16v3& v) { return hash_value<i16, glm::defaultp>(v); }
    inline size_t hash_value(const ui16v2& v) { return hash_value<ui16, glm::defaultp>(v); }
    inline size_t hash_value(const ui16v3& v) { return hash_value<ui16, glm::defaultp>(v); }
};

namespace std {
    inline size_t hash_value(const std::thread::id& v) { return std::hash<std::thread::id>()(v); }
    inline size_t hash_value(const std::pair<f32v3, f32v3>& v) {
        size_t seed = 0;
        boost::hash_combine(seed, v.first.x);
        boost::hash_combine(seed, v.first.y);
        boost::hash_combine(seed, v.first.z);
        boost::hash_combine(seed, v.second.x);
        boost::hash_combine(seed, v.second.y);
        boost::hash_combine(seed, v.second.z);
        return seed;
    };
};

// TODO Move
class f32v2hash {
public:
    size_t operator()(const f32v2& v) const {
        size_t seed = 0;
        boost::hash_combine(seed, v.x);
        boost::hash_combine(seed, v.y);
        return seed;
    }
};

class f32v3hash {
public:
    size_t operator()(const f32v3& v) const {
        size_t seed = 0;
        boost::hash_combine(seed, v.x);
        boost::hash_combine(seed, v.y);
        boost::hash_combine(seed, v.z);
        return seed;
    }
};

struct f32v2cmp {
    bool operator()(const f32v2& a, const f32v2& b) const {
        if (a.x < b.x) return true;
        if (a.x > b.x) return false;
        if (a.y < b.y) return true;
        return false;
    }
};

struct f32v3cmp {
    bool operator()(const f32v3& a, const f32v3& b) const {
        if (a.x < b.x) return true;
        if (a.x > b.x) return false;
        if (a.y < b.y) return true;
        if (a.y > b.y) return false;
        if (a.z < b.z) return true;
        return false;
    }
};