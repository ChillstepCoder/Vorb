#include "stdafx.h"
#include "Blendspace1DRepository.h"

#include "rendering/model/skeletal/AnimVariables.h"

void Blendspace1DRepository::onRegisteredAsset(AssetID id) {
    Blendspace1DDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
}

AssetLoadFunc Blendspace1DRepository::getAssetLoadFunc() {
    return[&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        Blendspace1DDef& def = *static_cast<Blendspace1DDef*>(assetDataPtr);

        for (auto& node : def.nodes) {
            if (node.animation.isValid()) [[likely]] {
                def.addDependency(node.animation.getAssetHandleBase());
            }
        }

        if (!def.getDependencies()) {
            fixupLoadedAsset(assetID);
            return true;
        }
        assetLoader.requestAssetLoadWithDependencies([this]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
            fixupLoadedAsset(assetID);
            return true;
        },
            nullptr,
            assetID,
            assetDataPtr,
            filePath,
            mLoadedAssets[assetID].get(),
            nullptr,
            def.getDependencies()
        );
        return false;
    };
}

void Blendspace1DRepository::fixupLoadedAsset(AssetID assetId) {
    Blendspace1DDef& def = getMutableAssetInternal(assetId);

    def.playerNodes.clear();
    def.playerNodes.reserve(def.nodes.size());
    for (auto& node : def.nodes) {
        if (node.animation.isValid()) [[likely]] {
            def.playerNodes.emplace_back(Blendspace1DPlayerNode{ node.x, node.animation.getAssetID() });
        }
    }
    switch (def.inputBinding.bindingType) {
        case AnimVariableFloatBindingType::VelocityX:
            def.inputBindingRuntime.byteOffset = offsetof(AnimVariables, velocity2d.x);
            break;
        case AnimVariableFloatBindingType::VelocityY:
            def.inputBindingRuntime.byteOffset = offsetof(AnimVariables, velocity2d.y);
            break;
        case AnimVariableFloatBindingType::Speed:
            def.inputBindingRuntime.byteOffset = offsetof(AnimVariables, speed);
            break;
        default:
            panic("Missing binding type");
            break;

    }
    def.inputBindingRuntime.rangeStart = def.inputBinding.range.x;
    assert(def.inputBinding.range.x != def.inputBinding.range.y);
    def.inputBindingRuntime.inverseRange = 1.0f / (def.inputBinding.range.y - def.inputBinding.range.x);
}
