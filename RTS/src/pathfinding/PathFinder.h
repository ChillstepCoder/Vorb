#pragma once

class World;

typedef ui32v2 PathPoint;

// TODO: Can this be contiguous?
struct Path {
    std::unique_ptr<PathPoint[]> points;
    ui32 numPoints = 0;
    bool finishedGenerating = false;
};


// TODO: Memory recycler for path memory
class PathCache {
    
};

// TODO: Also support flow path finding for large group movements, such as for moving in formation
class PathFinder {
public:
    PathFinder() {};

    // Fine grid paths
    std::unique_ptr<Path> generatePathSynchronous(const World& world, const ui32v2& start, const ui32v2& goal);
    void generatePathAsynchronous();

    // Coarse grid paths
    void generateLODChunkPathSynchronous();
};

