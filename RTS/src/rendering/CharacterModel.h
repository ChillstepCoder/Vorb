#pragma once

#include <Vorb/graphics/Texture.h>

#include "ecs/component/CharacterControlComponent.h"
#include "rendering/model/ModelConst.h"
#include "ecs/component/ComponentDefBase.h"

enum CharacterModelTextureIndex {
	CHARACTER_MODEL_TEXTURE_FRONT = 0,
	CHARACTER_MODEL_TEXTURE_SIDE  = 1,
	CHARACTER_MODEL_TEXTURE_BACK  = 2
};

struct CharacterModelComponent {
    CharacterModelComponent(ModelID modelId, AssetID animMachineID) : modelId(modelId), animMachineID(animMachineID) {}

    ModelID modelId = INVALID_MODEL_ID;
	AssetID animMachineID = INVALID_ASSET_ID;
};
static_assert(sizeof(CharacterModelComponent) == 8, "Keep small");

class CharacterModelComponentDef : public ComponentDefBase {
public:
	SoftAssetReference model = SoftAssetReference(AssetType::Model);
	SoftAssetReference animMachine = SoftAssetReference(AssetType::AnimMachine);
};
SERIALIZABLE_SIMPLE(CharacterModelComponentDef,
	make_field(o.model, "model"sv),
    make_field(o.animMachine, "anim_mach"sv)
);
