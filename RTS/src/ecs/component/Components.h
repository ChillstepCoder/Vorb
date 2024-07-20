#pragma once

#include "ecs/component/ComponentType.h"

// All serializable component includes
#include "ecs/component/PlayerControlComponent.h"
#include "ecs/component/CharacterControlComponent.h"
#include "ecs/component/CombatComponent.h"
#include "ecs/component/CorpseComponent.h"
#include "ecs/component/CharacterDetailsComponent.h"
#include "ecs/component/DynamicLightComponent.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/FullBrainComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/ProfessionComponent.h"
#include "ecs/component/TimedTileInteractComponent.h"
#include "ecs/component/DualInventoryComponent.h"
#include "ecs/component/SkillsComponent.h"
#include "ecs/component/CameraAttachComponent.h"
#include "ecs/component/PositionComponent.h"
#include "ecs/component/DynamicModelComponent.h"
#include "ecs/component/StaticModelComponent.h"
#include "ecs/component/TileItemComponent.h"
#include "ecs/component/ProjectileComponent.h"
#include "ecs/component/PlayerIdComponent.h"
#include "ecs/component/SimpleTextNameplateComponent.h"
#include "ecs/component/ObjectPickupComponent.h"
#include "ecs/component/EntityUidComponent.h"
#include "ecs/business/BusinessComponent.h"
// Charactermodel has a component TODO: Split
#include "rendering/CharacterModel.h"

static_assert(e_count(ComponentType) == 12, "Update component includes");

