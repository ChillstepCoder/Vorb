#pragma once

#include <Vorb/graphics/Texture.h>

#include "ecs/component/CharacterControlComponent.h"
#include "rendering/model/ModelConst.h"
#include "ecs/component/ComponentDefBase.h"

#include <Vorb/Event.hpp>

struct LinkedSubmodel {
    StrToken attachBone;
    ModelID submodelId = INVALID_MODEL_ID;
    ui8 cachedAttachBoneIndex = 0;

    bool operator==(const LinkedSubmodel& other) const {
        return attachBone == other.attachBone && submodelId == other.submodelId;
    }
};

enum CharacterModelTextureIndex {
	CHARACTER_MODEL_TEXTURE_FRONT = 0,
	CHARACTER_MODEL_TEXTURE_SIDE  = 1,
	CHARACTER_MODEL_TEXTURE_BACK  = 2
};

enum class CharacterModelEventType {
    SubmodelAdded,
    SubmodelRemoved,
};
struct CharacterModelEvent {
    entt::entity owner; // TODO: UID?
    LinkedSubmodel submodel;
};
EVENT_DISPATCHER_TYPE(CharacterModel, CharacterModelEventType, const CharacterModelEvent&);

class CharacterModelEvents {
public:
    STATIC_EVENT_LISTENER_FUNCS(CharacterModel, SubmodelAdded, CharacterModelEventType::SubmodelAdded, const CharacterModelEvent&);
    STATIC_EVENT_LISTENER_FUNCS(CharacterModel, SubmodelRemoved, CharacterModelEventType::SubmodelRemoved, const CharacterModelEvent&);
    STATIC_EVENT_DISPATCHER_DEF(CharacterModel);
};

struct CharacterModelComponent {
    CharacterModelComponent(ModelID modelId) : modelId(modelId) {}

    ModelID modelId = INVALID_MODEL_ID;
};
static_assert(sizeof(CharacterModelComponent) == 4, "Keep small");

struct CharacterLinkedSubmodelComponent {

    void addLinkedSubmodel(entt::entity thisEntity, LinkedSubmodel linkedSubmodel);
    void removeLinkedSubmodel(entt::entity thisEntity, LinkedSubmodel linkedSubmodel);
    void toggleLinkedSubmodel(entt::entity thisEntity, LinkedSubmodel linkedSubmodel);
    bool hasLinkedSubmodel(LinkedSubmodel submodel);
    const std::vector<LinkedSubmodel>& getLinkedSubmodels() { return linkedSubmodels; }
private:
    std::vector<LinkedSubmodel> linkedSubmodels;
};

class CharacterModelComponentDef : public ComponentDefBase {
public:
    ModelAssetRef model;
};
SERIALIZABLE_SIMPLE(CharacterModelComponentDef,
	make_field(o.model, "model"sv)
);

struct CharacterBodyMorphData {
    f32 muscular = 0.0f;
    f32 fat = 0.0f;
};

// TODO: USE
class ModularCharacterModel {
public:
    ModularCharacterModel() = default;
    ModularCharacterModel(ModelID headModelId, ModelID bodyModelId, ModelID armsModelId, ModelID legsModelId, ModelID hairModelId) :
        mHeadModelId(headModelId),
        mBodyModelId(bodyModelId),
        mArmsModelId(armsModelId),
        mLegsModelId(legsModelId),
        mHairModelId(hairModelId) {
    }
	ModelID mHeadModelId = INVALID_MODEL_ID;
    ModelID mBodyModelId = INVALID_MODEL_ID;
    ModelID mArmsModelId = INVALID_MODEL_ID;
	ModelID mLegsModelId = INVALID_MODEL_ID;
	ModelID mHairModelId = INVALID_MODEL_ID;

    CharacterBodyMorphData mBodyMorphData;
};