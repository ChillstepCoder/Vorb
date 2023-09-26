#pragma once
#include "rendering/CharacterModel.h"

#include "definitions/ModelDef.h"
#include "events/SkillEvent.h"

class MaterialShaderDef;
class Camera3D;
struct CharacterRenderState;

class CharacterRenderData {
public:
    CharacterRenderData();
    ~CharacterRenderData();

    CharacterAnimState mAnimState;
    // TODO: Not ptr, we can just have AssetHandle and aquire
    AssetHandlePtr<ModelDef> mModelHandle;
    bool mIsInitialized = false;
};

class CharacterRenderer {
public:
	CharacterRenderer();
	~CharacterRenderer();

    void addCharacterModel(entt::entity entityId, AssetID modelId);
    void removeCharacterModel(entt::entity entityId);

    void playOneShotAnimation(entt::entity entityId, AssetID animationId);
    void renderCharacters(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha);
private:
    bool tryInitializeCharacterAnimState(entt::entity entityId);

    AssetHandlePtr<MaterialShaderDef> mShaderHandle;
    std::unordered_map<entt::entity, std::unique_ptr<CharacterRenderData>> mEntityCharacterModels;

};

//  TODO: This is temp af
//extern vg::Texture sShadowTexture;

