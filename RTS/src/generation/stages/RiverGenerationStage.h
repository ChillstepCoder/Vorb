#pragma once
#include "IWorldGenerationStage.h"



class RiverGenerationStage : public IWorldGenerationStage
{
public:
    using IWorldGenerationStage::IWorldGenerationStage;

    void begin() override;

    const char* getStageName() const override { return "Rivers"; }

    bool update() override;

protected:
    void generateRiverPath(size_t riverIndex);

    std::atomic<ui32> mNumFinishedRiverPaths = 0;
};

