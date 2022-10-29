#pragma once
#include "rendering/CharacterModel.h"

class MaterialRenderer;
class PhysicsComponent;
class Material;
struct CharacterModelComponent;
struct CharacterControlComponent;
class Camera3D;
struct CharacterRenderState;

class CharacterRenderer {
public:
	CharacterRenderer();
	~CharacterRenderer();

    void addCharacterModel(entt::entity entityId, ui32 modelId);
    void removeCharacterModel(entt::entity entityId);
    void renderCharacters(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha, const MaterialRenderer& materialRenderer);
private:

    const Material* mMaterial;
    std::unordered_map<entt::entity, std::unique_ptr<AnimState>> mEntityCharacterModels;
};

//  TODO: This is temp af
//extern vg::Texture sShadowTexture;

