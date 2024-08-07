#pragma once
#include "rendering/CharacterModel.h"

#include "definitions/ModelDef.h"
#include "events/SkillEvent.h"

#include "rendering/gl/GpuStreamingDataBuffer.h"
#include "rendering/renderstate/DynamicModelInstanceState.h"

class MaterialShaderDef;
class Camera3D;
class World;
struct CharacterRenderState;
struct CharacterRendererCharacterState;

typedef std::unordered_map<entt::entity, CharacterRendererCharacterState> EntityCharacterModelMap;

struct CharacterModelRendererData {
    EntityCharacterModelMap entityCharacterModels;
    AssetHandlePtr<ModelDef> handle;
    bool needsInitialize = true;
    std::unique_ptr<GpuStreamingDataBuffer> boneTransformsBuffer; // model matrix followed by bone transforms
};

enum class CharacterModelUpdateType : ui8 {
    Add,
    Remove,
    AddSubmodel,
    RemoveSubmodel,
    COUNT
};

struct CharacterModelUpdateData {
    CharacterModelUpdateData() : entityId(entt::null), modelId(INVALID_MODEL_ID), type(CharacterModelUpdateType::COUNT) {}
    CharacterModelUpdateData(entt::entity entityId, ui32 modelId, CharacterModelUpdateType type) : entityId(entityId), modelId(modelId), type(type) {}
    CharacterModelUpdateData(entt::entity entityId, LinkedSubmodel submodel, CharacterModelUpdateType type) : entityId(entityId), submodel(submodel), type(type) {}

    entt::entity entityId;
    union {
        ui32 modelId;
        LinkedSubmodel submodel;
    };
    CharacterModelUpdateType type;
};

// TODO: Split into CharacterModelManager and CharacterRenderer?
class CharacterRenderer {
public:
	CharacterRenderer();
	~CharacterRenderer();

    void onWorldBegin(World& world);
    void frameBegin();

    void addCharacterModel(entt::entity entityId, AssetID modelId);
    void removeCharacterModel(entt::entity entityId, AssetID modelId);

    void playOneShotAnimation(entt::entity entityId, AssetID animationId);
    void renderCharactersAndGatherSubmodels(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha, std::vector<DynamicModelInstanceState>& outLinkedSubmodels);

    CharacterRendererCharacterState* tryGetCharacterRenderStateForDebug(entt::entity entityId);
private:
    void addCharacterModelInternal(entt::entity entityId, AssetID modelId);
    void removeCharacterModelInternal(entt::entity entityId, AssetID modelId);
    void addSubmodelInternal(entt::entity entityId, LinkedSubmodel submodel);
    void removeSubmodelInternal(entt::entity entityId, LinkedSubmodel submodel);

    void onCharacterModelConstruct(entt::registry& registry, entt::entity entity);
    void onCharacterModelDestroy(entt::registry& registry, entt::entity entity);
    void onCharacterModelSubmodelAdded(entt::entity entity, LinkedSubmodel submodel);
    void onCharacterModelSubmodelRemoved(entt::entity entity, LinkedSubmodel submodel);

    moodycamel::ConcurrentQueue<CharacterModelUpdateData> mModelsToUpdate;

    AssetHandlePtr<MaterialShaderDef> mShaderHandle;

    std::map<ModelID, CharacterModelRendererData> mModelRenderData;
    std::unordered_map<entt::entity, CharacterRendererCharacterState*> mEntityCharacterRenderData;

    CharacterModelListeners mCharacterModelListeners;

    entt::registry* mRegistry = nullptr;
};
