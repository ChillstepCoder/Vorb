#pragma once

class ResourceManager;
class Camera3D;
class BuildingMesher;
class TextureAtlas;
class MaterialRenderer;

class BuildingRenderer
{
public:
    BuildingRenderer(ResourceManager& resourceManager, const MaterialRenderer& materialRenderer);
    ~BuildingRenderer();

private:
    std::unique_ptr<BuildingMesher> mMesher;
    ResourceManager& mResourceManager;

    const MaterialRenderer& mMaterialRenderer;
};

