#pragma once

typedef ui16v2 PathPoint;

struct NavPath {

    NavPath() = default;
    ~NavPath() { assert(finishedGenerating == true); }

    bool isInvalid() const { return points == nullptr; }

    std::unique_ptr<PathPoint[]> points;
    ui32 numPoints = 0;
    std::atomic_bool finishedGenerating = false;
};


