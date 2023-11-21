#pragma once

#include "definitions/rendering/TextureDef.h"

class EditorResources
{
public:
    static void loadAllResources();
    static void freeAllResources();
    static bool isFullyLoaded();

    const TextureDef* tryGetIconForAssetType(AssetType type);

    inline static AssetHandlePtr<TextureDef> animIcon;
    inline static AssetHandlePtr<TextureDef> tileIcon;
    inline static AssetHandlePtr<TextureDef> fbxIcon;
    inline static AssetHandlePtr<TextureDef> folderIcon;
    inline static AssetHandlePtr<TextureDef> fileIcon;
    inline static AssetHandlePtr<TextureDef> fontIcon;
    inline static AssetHandlePtr<TextureDef> materialIcon;
    inline static AssetHandlePtr<TextureDef> meshIcon;
    inline static AssetHandlePtr<TextureDef> pngIcon;
    inline static AssetHandlePtr<TextureDef> psysIcon;
    inline static AssetHandlePtr<TextureDef> skelIcon;
    inline static AssetHandlePtr<TextureDef> effectIcon;
    inline static AssetHandlePtr<TextureDef> cubeIcon;
    inline static AssetHandlePtr<TextureDef> animGraphIcon;
    inline static AssetHandlePtr<TextureDef> skillIcon;
    inline static AssetHandlePtr<TextureDef> itemIcon;
    inline static AssetHandlePtr<TextureDef> fishIcon;
    inline static AssetHandlePtr<TextureDef> shaderIcon;
    inline static AssetHandlePtr<TextureDef> floraIcon;
    inline static AssetHandlePtr<TextureDef> biomeIcon;

    inline static AssetHandlePtr<TextureDef> backIcon;
    inline static AssetHandlePtr<TextureDef> forwardIcon;
    inline static AssetHandlePtr<TextureDef> clearIcon;
    inline static AssetHandlePtr<TextureDef> gearIcon;
    inline static AssetHandlePtr<TextureDef> searchIcon;

    inline static AssetHandlePtr<TextureDef> shadowTexture;
    inline static AssetHandlePtr<TextureDef> translucencyTexture;

private:
    inline static bool hasLoaded = false;
    inline static std::vector<AssetHandlePtr<TextureDef>*> allResources;
    inline static std::unordered_map<AssetType, const AssetHandlePtr<TextureDef>*> assetIconLookup;
};

