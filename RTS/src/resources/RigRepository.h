#pragma once

#include "definitions/RigDef.h"

class AnimationRepository;
DECL_VIO(class IOManager);

class RigRepository
{
public:
    RigRepository(vio::IOManager& ioManager);
    ~RigRepository();

    bool loadRigFile(const vio::Path& filePath, const AnimationRepository& animRepo);

    const RigDef& getRigDef(ui32 rigId) const { return mRigDefs[rigId]; }
    const RigDef& getRigDef(const nString& name) const;
    const RigDef* tryGetRigDef(const nString& name) const;

private:

    vio::IOManager& mIoManager;
    std::unordered_map<nString, ui32> mRigIdLookup;
    std::vector<RigDef> mRigDefs;
};

