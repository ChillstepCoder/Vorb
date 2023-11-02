#pragma once

#include "resources/IAsset.h"

#include "serialization/YmlSerializer.h"

class SoftAssetReference {
public:
    SoftAssetReference(AssetType assetType) : assetType(assetType) {};

    bool isValid() const { return name.isValid(); }

    StrToken name;
    const AssetType assetType;
};

YML_WRITE_DEF(SoftAssetReference) {
    ryml::NodeRef& nr = *n;
    nr << o.name.toString();
}
YML_READ_DEF(SoftAssetReference) {
    c4::csubstr str;
    n >> str;
    if (str.size() > MAX_CHARS_IN_STRTOKEN) {
        panic("Invalid asset reference strtoken length (max 20) {} {}", str.size(), str.data());
    };
    target->name = StrToken(str.data(), str.size());
    return true;
}