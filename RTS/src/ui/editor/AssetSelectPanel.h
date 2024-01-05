#pragma once

#include <variant>
class ModelDef;
struct MaterialGpuData;
struct TileGrassDef;
struct FishDef;
class MaterialShaderDef;
class ParticleSystemDef;

#include "resources/asset/AssetHandleBundle.h"

DECL_VG(class GBuffer);

enum class AssetSelectPanelResultCode {
    NONE,
    EDIT_ASSET,
    COUNT
};

typedef std::pair<AssetSelectPanelResultCode, AssetDescriptor> AssetSelectPanelResult;

class AssetSelectPanel
{
public:
    AssetSelectPanel();
    ~AssetSelectPanel();

    AssetSelectPanelResult updateAndRender(float ySize);
};

