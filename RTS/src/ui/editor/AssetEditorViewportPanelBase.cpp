#include "stdafx.h"
#include "AssetEditorViewportPanelBase.h"

#include "rendering/MaterialShaderRepository.h"

AssetEditorViewportPanelBase::AssetEditorViewportPanelBase() = default;
AssetEditorViewportPanelBase::~AssetEditorViewportPanelBase() = default;

const MaterialShaderDef* AssetEditorViewportPanelBase::getModelRenderShader() const
{
    switch (mDrawMode) {
        case EditorViewportDrawMode::PBRTest:
            if (!mPbrMaterial) mPbrMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model_pbr"));
            return mPbrMaterial->tryGetLoadedAsset();
        case EditorViewportDrawMode::BlendTest:
        case EditorViewportDrawMode::EdgeTest:
        case EditorViewportDrawMode::Lit:
        case EditorViewportDrawMode::Unlit:
        case EditorViewportDrawMode::Normals:
        case EditorViewportDrawMode::Tangents:
        case EditorViewportDrawMode::AO:
        case EditorViewportDrawMode::Metallic:
        case EditorViewportDrawMode::Roughness:
        case EditorViewportDrawMode::UVs:
            if (!mEditorMaterial) mEditorMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model"));
            return mEditorMaterial->tryGetLoadedAsset();
        case EditorViewportDrawMode::Wireframe:
            if (!mWireframeMaterial) mWireframeMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("mesh_wireframe"));
            return mWireframeMaterial->tryGetLoadedAsset();
        default:
            assert(false);
            break;
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12);
    return nullptr;
}
