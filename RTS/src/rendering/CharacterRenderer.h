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

enum class CharacterModelUpdateType : ui8 {
    Add,
    Remove,
    AddSubmodel,
    RemoveSubmodel,
    COUNT
};

struct CharacterModelUpdateData {
    CharacterModelUpdateData() : entityId(entt::null), type(CharacterModelUpdateType::COUNT) {}
    CharacterModelUpdateData(entt::entity entityId, CharacterModelUpdateType type) :
        entityId(entityId), type(type) {}
    CharacterModelUpdateData(entt::entity entityId, ModularHumanoidCharacterModel model, std::vector<LinkedSubmodelData>&& submodels) :
        entityId(entityId), model(model), submodels(std::move(submodels)), type(CharacterModelUpdateType::Add) {}
    CharacterModelUpdateData(entt::entity entityId, LinkedSubmodelData submodel, CharacterModelUpdateType type) :
        entityId(entityId), submodels({ submodel }), type(type) {}

    // We could flyweight this a bit with pointer to data if we cared but
    // these dont get sent much so who cares

    entt::entity entityId;
    ModularHumanoidCharacterModel model;
    std::vector<LinkedSubmodelData> submodels;
    CharacterModelUpdateType type;
};

// TODO: Split into CharacterModelManager and CharacterRenderer?
// TODO: Utilize entity UID instead of entt::entity since entities can be re-used?
class CharacterRenderer {
public:
	CharacterRenderer();
	~CharacterRenderer();

    void onWorldBegin(World& world);
    void frameBegin();

    void addCharacterModel(entt::entity entityId, ModularHumanoidCharacterModel modularCharacter, std::vector<LinkedSubmodelData> submodels);
    void removeCharacterModel(entt::entity entityId);

    void playOneShotAnimation(entt::entity entityId, AssetID animationId);
    void renderCharactersAndGatherSubmodels(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha, DynamicModelInstanceStateContainer& outLinkedSubmodels);

    CharacterRendererCharacterState* tryGetCharacterRenderStateForDebug(entt::entity entityId);
private:
    void addCharacterModelInternal(entt::entity entityId, const ModularHumanoidCharacterModel& modularCharacter, std::vector<LinkedSubmodelData>&& submodels);
    void removeCharacterModelInternal(entt::entity entityId);
    void addSubmodelInternal(entt::entity entityId, LinkedSubmodelData submodel);
    void removeSubmodelInternal(entt::entity entityId, LinkedSubmodelData submodel);

    void onCharacterModelConstruct(entt::registry& registry, entt::entity entity);
    void onCharacterModelDestroy(entt::registry& registry, entt::entity entity);
    void onCharacterModelSubmodelAdded(entt::entity entity, LinkedSubmodelData submodel);
    void onCharacterModelSubmodelRemoved(entt::entity entity, LinkedSubmodelData submodel);

    moodycamel::ConcurrentQueue<CharacterModelUpdateData> mModelsToUpdate;

    AssetHandlePtr<MaterialShaderDef> mShaderHandle;

    UnorderedFlatMap<entt::entity, CharacterRendererCharacterState> mCharacterModels;

    CharacterModelListeners mCharacterModelListeners;

    entt::registry* mRegistry = nullptr;

    struct SubmeshInstanceData {
        ui32 modelTransformIndex;
        ui32 boneTransformStartIndex;
        ui32 variantIndex;
    };

    // TODO: SHADOWS
    // TODO: Other shader support
    std::unique_ptr<GLDrawCommandBuffer> mDrawCommands;

    std::unique_ptr<GpuStreamingDataBuffer> mBoneTransformsBuffer;
    std::unique_ptr<GpuStreamingDataBuffer> mModelTransformsBuffer;
    std::unique_ptr<GpuStreamingDataBuffer> mSubmeshDataBuffer;
    ui32 mTotalJointTransforms = 0;
    ui32 mTotalSubmeshParts = 0;

    std::unique_ptr<moodycamel::ConsumerToken> mConsumerToken;
    std::mutex mOutLinkedSubmodelsMutex;

};
