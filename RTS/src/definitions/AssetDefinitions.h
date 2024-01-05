#pragma once

#include "definitions/AnimationDef.h"
#include "definitions/AnimMachineDef.h"
#include "definitions/BiomeDef.h"
#include "definitions/BrushDef.h"
#include "definitions/BuildingDef.h"
#include "definitions/BusinessDef.h"
#include "definitions/EffectDef.h"
#include "definitions/FishDef.h"
#include "definitions/RigDef.h"
#include "definitions/ModelDef.h"
#include "definitions/ParticleSystemDef.h"
#include "definitions/rendering/CubemapDef.h"
#include "definitions/rendering/TextureDef.h"
#include "definitions/SkillDef.h"
#include "definitions/TileDef.h"
#include "definitions/TileGrassDef.h"
#include "definitions/TileDistributionDef.h"
#include "item/ItemDef.h"

static_assert(e_count(AssetType) == 18, "Add all includes here");