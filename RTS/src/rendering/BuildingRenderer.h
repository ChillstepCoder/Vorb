#pragma once

class Building;
class Camera3D;
class TextureAtlas;
class Material;

class BuildingRenderer
{
public:
    BuildingRenderer();

    void renderBuildingRoof(const Building& building, const Camera3D& camera);
    void renderBuildingShadows(const Building& building, const Camera3D& camera);

private:

    const Material* mRoofMaterial = nullptr;
    const Material* mRoofBaseMaterial = nullptr;
    const Material* mRoofShadowMaterial = nullptr;
};

