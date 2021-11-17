#pragma once

class ResourceManager;
class Building;
class Camera3D;
class BuildingMesher;
class TextureAtlas;
class MaterialRenderer;
class Material;

class BuildingRenderer
{
public:
    BuildingRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer);
    ~BuildingRenderer();

    void renderBuildingRoof(const Building& building);
    void renderBuildingRoofShadows(const Building& building);

private:

    std::unique_ptr<BuildingMesher> mMesher;
    ResourceManager& mResourceManager;

    const MaterialRenderer& mMaterialRenderer;
    const Material* mRoofMaterial = nullptr;
    const Material* mRoofBaseMaterial = nullptr;
    const Material* mRoofShadowMaterial = nullptr;
};

