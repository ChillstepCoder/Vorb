#pragma once

#include <Vorb/graphics/Texture.h>

#include "ecs/component/CharacterControlComponent.h"
#include "rendering/model/ModelConst.h"
#include "ecs/component/ComponentDefBase.h"

#include <Vorb/Event.hpp>


enum class HumanoidSubmeshPart {
    Feet,
    Legs,
    Torso,
    Arms,
    Head,
    EyeL,
    EyeR,
    COUNT
};

struct ModularHumanoidCharacterModel {
    ModularHumanoidCharacterModel() {
        for (auto& id : partIds) id = INVALID_SUBMESH_ID;
    }

    ModelID baseModel = INVALID_MODEL_ID;
    std::array<SubmeshID, e_count(HumanoidSubmeshPart)> partIds;
};

struct LinkedSubmodelData {
    ModelID modelId;
    ui8 attachBoneIndex;

    auto operator<=>(const LinkedSubmodelData&) const = default;
};

struct LinkedSubmodelDesc {
    StrToken attachBone; // TODO: Just bone ID?
    ModelID modelId = INVALID_MODEL_ID;

    bool operator==(const LinkedSubmodelDesc& other) const {
        return attachBone == other.attachBone && modelId == other.modelId;
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
    entt::entity owner; // TODO: UID? Reuse could be an issue
    LinkedSubmodelData submodel;
};
EVENT_DISPATCHER_TYPE(CharacterModel, CharacterModelEventType, const CharacterModelEvent&);

class CharacterModelEvents {
public:
    STATIC_EVENT_LISTENER_FUNCS(CharacterModel, SubmodelAdded, CharacterModelEventType::SubmodelAdded, const CharacterModelEvent&);
    STATIC_EVENT_LISTENER_FUNCS(CharacterModel, SubmodelRemoved, CharacterModelEventType::SubmodelRemoved, const CharacterModelEvent&);
    STATIC_EVENT_DISPATCHER_DEF(CharacterModel);
};

struct CharacterModelComponent {
    CharacterModelComponent(ModularHumanoidCharacterModel model, const std::vector<LinkedSubmodelDesc>* inLinkedSubmodels);

    void addLinkedSubmodel(entt::entity thisEntity, LinkedSubmodelDesc linkedSubmodel);
    void removeLinkedSubmodel(entt::entity thisEntity, LinkedSubmodelDesc linkedSubmodel);
    void toggleLinkedSubmodel(entt::entity thisEntity, LinkedSubmodelDesc linkedSubmodel);
    bool hasLinkedSubmodel(LinkedSubmodelData submodel);
    ModularHumanoidCharacterModel getModel() { return model; }
    const std::vector<LinkedSubmodelData>& getLinkedSubmodels() { return linkedSubmodels; }

private:
    ModularHumanoidCharacterModel model;
    std::vector<LinkedSubmodelData> linkedSubmodels;
};

class CharacterModelComponentDef : public ComponentDefBase {
public:
    ModelAssetRef model;
};
SERIALIZABLE_SIMPLE(CharacterModelComponentDef,
	make_field(o.model, "model"sv)
);
