#include "stdafx.h"
#include "IWorldGenerationStage.h"

#include "world/host/HostWorldData.h"
#include "generation/WorldDataGenerator.h"

IWorldGenerationStage::IWorldGenerationStage(WorldDataGenerator& generator)
    : mGenerator(generator), mGenerationData(generator.getGenerationData()), mBlackboard(generator.getBlackboard()) {

    mWorldData = mGenerator.getWorldData();
    assert(mWorldData);
    mHeightGrid = mWorldData->heightmapGrid.get();
    mBiomeGrid = mWorldData->biomeGrid.get();
    mWorldSeed = mGenerationData.mSeedHashed;
    mWorldSeedInt = mGenerationData.mSeedInt;
    mTotalHeightPatches = mWorldData->heightmapGrid->getTotalPatches();
    mMappedHeights = mGenerator.getMappedHeights();
    mMappedBiomes = mGenerator.getMappedBiomes();

    mBiomeTexture = mGenerator.getBiomeTexture();
    mBiomeSSBO = mGenerator.getBiomeSSBO();
    mHeightTexture = mGenerator.getHeightTexture();
    mHeightSSBO = mGenerator.getHeightSSBO();

    assert(mMappedHeights);
    assert(mMappedBiomes);
}

void IWorldGenerationStage::abort() {
    // Trigger all threads to stop functioning
    ++sGenerationUID;
    Services::Threadpool::ref().clearTasks();
    // Wait for all threads to finish
    while (Services::Threadpool::ref().getNumRunningThreads()) {
        Sleep(1);
    }

    abortInternal();
}

