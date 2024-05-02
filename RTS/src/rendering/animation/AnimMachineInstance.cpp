#include "stdafx.h"
#include "AnimMachineInstance.h"

#include "resources/AnimMachineRepository.h"

AnimMachineInstance::AnimMachineInstance(AssetID animMachineID) {
    machineDefHandle = AnimMachineRepository::get().getAssetHandle(animMachineID);
}
