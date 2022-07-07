#pragma once

class Building;

class BuildingNavGraph
{
    friend class PathFinder;
public:
    BuildingNavGraph(Building& building);
    ~BuildingNavGraph();

    void update();

private:
    Building& mBuilding;
    // We store the entire nav graph in a single block of memory for fast traversal
    ui32* mNavData = nullptr;
    ui32 mNumNodes = 0;
};

