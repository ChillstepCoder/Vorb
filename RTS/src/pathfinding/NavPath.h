#pragma once

#include "tile/TileHandle.h"

class IHeightmapGrid;

struct PathPoint {
    PathPoint() = default;
    PathPoint(ui16v2&& r) : xy(std::move(r)) {};
    PathPoint(const ui16v2& r) : xy(r) {};
    PathPoint(const f32v2& r) : xy(r) {};
    PathPoint(const ui32v2& r) : xy(r) {};
    PathPoint(ui16 x, ui16 y) : xy(x, y) {};
    PathPoint(const f32v3& r) : xy(r.x, r.y) {};

    PathPoint& operator+=(const PathPoint& r) {
        xy += r.xy;
        return *this;
    }
    PathPoint& operator-=(const PathPoint& r) {
        xy -= r.xy;
        return *this;
    }
    PathPoint operator-(const PathPoint& r) const {
        return PathPoint(xy - r.xy);
    }
    PathPoint operator+(const PathPoint& r) const {
        return PathPoint(xy + r.xy);
    }
    PathPoint operator*(ui16 r) const {
        return PathPoint(x * r, y * r);
    }
    PathPoint& operator=(const ui16v2& r) {
        xy = r;
        return *this;
    }
    bool operator!=(const ui16v2& r) const {
        return xy != r;
    }
    bool operator==(const ui16v2& r) const {
        return xy == r;
    }

    explicit operator ui16v2 () const { return xy; }
    explicit operator ui16v2& () { return xy; }
    explicit operator const ui16v2& () const { return xy; }

    union {
        ui16v2 xy;
        struct {
            ui16 x;
            ui16 y;
        };
    };
};


struct NavPathPoint {
    f32v3 pos;
    bool isSim;
};

class NavPath {
    friend class PathFinder;
public:
    NavPath() = default;
    ~NavPath() {
        // Explicit delete the pointer
        assert(finishedGenerating == true);
        if (numPoints) {
            freePath();
        }
    };

    VORB_NON_COPYABLE(NavPath);

    NavPath(NavPath&& other) noexcept;
    NavPath& operator=(NavPath&& other) noexcept;

    bool isInvalid() const { return points == nullptr; }
    void allocatePath(ui32 numPoints);
    void freePath();

    // Allocate optimization
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t count);

    const NavPathPoint* getPoints() const { return points; }
    ui32 getNumPoints() const { return numPoints; }
    f32v3 getTargetPosition() const { return targetPosition; }

    std::vector<f32v3> convertToWorldPoints(const IHeightmapGrid& heightGrid) const;

private:
    f32v3 targetPosition;
    ui32 numPoints = 0;
    NavPathPoint* points = nullptr; // Raw pointer
public:
    // Atomic access check
    std::atomic_bool finishedGenerating = false;
};
static_assert(sizeof(NavPath) == 32);
