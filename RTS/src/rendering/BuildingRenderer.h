#pragma once

class Building;
class Camera3D;
class TextureAtlas;
class MaterialRenderer;
class Material;

class BuildingRenderer
{
public:
    BuildingRenderer(const MaterialRenderer& materialRenderer);
    ~BuildingRenderer();

    void renderBuildingRoof(const Building& building, const Camera3D& camera);
    void renderBuildingShadows(const Building& building, const Camera3D& camera);

private:

    const MaterialRenderer& mMaterialRenderer;
    const Material* mRoofMaterial = nullptr;
    const Material* mRoofBaseMaterial = nullptr;
    const Material* mRoofShadowMaterial = nullptr;
};

