#include "stdafx.h"
#include "EditorResources.h"

#include "resources/TextureRepository.h"

#include "resources/asset/AssetHandleBundle.h"

std::unique_ptr<AssetHandleBundle> sAssetBundle;

#define ADD_TEXTURE_RES(var, name) \
    var = textureRepository.getAssetHandle(CStrToken(name)); \
    allResources.push_back(&var); \
    sAssetBundle->addAssetHandle(var->clone());

void EditorResources::loadAllResources()
{
    if (hasLoaded) return;
    hasLoaded = true;
    sAssetBundle = std::make_unique<AssetHandleBundle>();

    TextureRepository& textureRepository = TextureRepository::get();
    ADD_TEXTURE_RES(animIcon, "anim_icon");
    ADD_TEXTURE_RES(tileIcon, "tile_icon");
    ADD_TEXTURE_RES(fbxIcon, "fbx_icon");
    ADD_TEXTURE_RES(fileIcon, "file_icon");
    ADD_TEXTURE_RES(folderIcon, "folder_icon");
    ADD_TEXTURE_RES(fontIcon, "font_icon");
    ADD_TEXTURE_RES(materialIcon, "material_icon");
    ADD_TEXTURE_RES(meshIcon, "mesh_icon");
    ADD_TEXTURE_RES(pngIcon, "png_icon");
    ADD_TEXTURE_RES(psysIcon, "psys_icon");
    ADD_TEXTURE_RES(skelIcon, "skel_icon");
    ADD_TEXTURE_RES(effectIcon, "effect_icon");
    ADD_TEXTURE_RES(cubeIcon, "cubemap_icon");
    ADD_TEXTURE_RES(animGraphIcon, "anim_graph_icon");
    ADD_TEXTURE_RES(skillIcon, "skill_icon");
    ADD_TEXTURE_RES(itemIcon, "item_icon");
    ADD_TEXTURE_RES(fishIcon, "fish_icon");
    ADD_TEXTURE_RES(shaderIcon, "shader_icon");
    ADD_TEXTURE_RES(floraIcon, "flora_icon");
    ADD_TEXTURE_RES(biomeIcon, "biome_icon");

    ADD_TEXTURE_RES(backIcon, "back_icon");
    ADD_TEXTURE_RES(forwardIcon, "forward_icon");
    ADD_TEXTURE_RES(clearIcon, "clear_icon");
    ADD_TEXTURE_RES(gearIcon, "gear_icon");
    ADD_TEXTURE_RES(searchIcon, "search_icon");

    ADD_TEXTURE_RES(shadowTexture, "ui_shadow");
    ADD_TEXTURE_RES(translucencyTexture, "ui_translucency");
    static_assert(e_count(AssetType) == 18, "Load icon if needed");
}

void EditorResources::freeAllResources() {
    for (auto&& resPtr : allResources) {
        resPtr->reset();
    }
    hasLoaded = false;
    allResources.clear();
    sAssetBundle.reset();
    assetIconLookup.clear();
}

bool EditorResources::isFullyLoaded() {
    return sAssetBundle && sAssetBundle->areAllAssetsLoaded();
}

const TextureDef* EditorResources::tryGetIconForAssetType(AssetType type) {
    auto&& it = assetIconLookup.find(type);
    if (it == assetIconLookup.end()) return nullptr;
    return &((*it->second)->getLoadedAsset());
}
