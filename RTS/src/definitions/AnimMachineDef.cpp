#include "stdafx.h"
#include "AnimMachineDef.h"

#include "rendering/animation/AnimMachineInstance.h"

AnimMachineDef::AnimMachineDef(StrToken name, AssetID id) : IAsset(name, id) { }
AnimMachineDef::~AnimMachineDef() = default;