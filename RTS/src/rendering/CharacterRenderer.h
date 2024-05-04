#pragma once
#include "rendering/CharacterModel.h"

#include "definitions/ModelDef.h"
#include "events/SkillEvent.h"


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
};

struct CharacterModelUpdateData {
    entt::entity entityId;
    ui32 modelId;
    bool isAdd;
};
static_assert(sizeof(CharacterModelUpdateData) == 12);

class CharacterRenderer {
public:
	CharacterRenderer();
	~CharacterRenderer();

    void onWorldBegin(World& world);
    void frameBegin();

    void addCharacterModel(entt::entity entityId, AssetID modelId);
    void removeCharacterModel(entt::entity entityId, AssetID modelId);

    void playOneShotAnimation(entt::entity entityId, AssetID animationId);
    void renderCharacters(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha);

    CharacterRendererCharacterState* tryGetCharacterRenderStateForDebug(entt::entity entityId);
private:
    void addCharacterModelInternal(entt::entity entityId, AssetID modelId);
    void removeCharacterModelInternal(entt::entity entityId, AssetID modelId);

    void onCharacterModelConstruct(entt::registry& registry, entt::entity entity);
    void onCharacterModelDestroy(entt::registry& registry, entt::entity entity);

    moodycamel::ConcurrentQueue<CharacterModelUpdateData> mModelsToUpdate;

    AssetHandlePtr<MaterialShaderDef> mShaderHandle;

    std::map<ModelID, CharacterModelRendererData> mModelRenderData;
    std::unordered_map<entt::entity, CharacterRendererCharacterState*> mEntityCharacterRenderData;

    entt::registry* mRegisteredECS = nullptr;
};
