#pragma once

#include "util/RollTableBase.h"

// For rolling random item drops

class ItemRollTable : public RollTableBase<LiteAssetRef<AssetType::Item>> {
protected:
    void ymlWriteValue(c4::yml::NodeRef& s) const override
    {
        throw std::logic_error("The method or operation is not implemented.");
    }

    void ymlReadValue(c4::yml::ConstNodeRef const& n) override
    {
        throw std::logic_error("The method or operation is not implemented.");
    }
};

// TODO:
YML_WRITE_DEF(ItemRollTable) {
    o.ymlWrite(*n);
}

YML_READ_DEF(ItemRollTable) {
    return target->ymlRead(n);
}