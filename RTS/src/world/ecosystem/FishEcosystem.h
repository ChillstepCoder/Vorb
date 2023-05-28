#pragma once

class IWorld;

// TODO: MOVE TO FISHREPOSITORY
typedef ui32 FishID;

#include <boost/container/flat_map.hpp>

struct FishPopulation {
    FishID mFishId;
    int mNumFish;
};

struct ActiveFish {
    f32v3 mPosition;
    f32 mRotation;
    FishID mFishId;
};

struct FishCell {
    boost::container::flat_map<FishID, int> mPopulations;
    // HMMM
    BitArray spawnableTiles;
    int totalSpawnableTiles; // can derive max population and population pressure from this
    std::vector<ActiveFish> activeFish; // LOD rendered
};
//SIZER(FishCell);

class FishEcosystem
{
public:
    FishEcosystem(IWorld& world);
    ~FishEcosystem();

    void tickGameThread();

    IWorld& mWorld;
};

