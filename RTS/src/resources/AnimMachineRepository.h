#pragma once

#include "definitions/AnimMachineDef.h"

DECL_VIO(class IOManager);
class RigRepository;

class AnimMachineRepository
{
public:
    AnimMachineRepository(vio::IOManager& ioManager, const RigRepository& rigRepository);
    ~AnimMachineRepository();

    bool loadMachineFile(const vio::Path& filePath);

    const AnimMachineDef& getAnimMachineDef(ui32 rigId) const { return mAnimMachineDefs[rigId]; }
    const AnimMachineDef& getAnimMachineDef(const nString& name) const;
    const AnimMachineDef* tryGetAnimMachineDef(const nString& name) const;

private:

    const RigRepository& mRigRepository;
    vio::IOManager& mIoManager;
    std::map<nString, ui32> mAnimMachineIdLookup;
    std::vector<AnimMachineDef> mAnimMachineDefs;
};

