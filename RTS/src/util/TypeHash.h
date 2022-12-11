#pragma once


#include <boost/container_hash/hash.hpp>

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

class f32v3pairhash {
public:
    size_t operator()(const std::pair<f32v3, f32v3>& v) const {
        size_t seed = 0;
        boost::hash_combine(seed, v.first.x);
        boost::hash_combine(seed, v.first.y);
        boost::hash_combine(seed, v.first.z);
        boost::hash_combine(seed, v.second.x);
        boost::hash_combine(seed, v.second.y);
        boost::hash_combine(seed, v.second.z);
        return seed;
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