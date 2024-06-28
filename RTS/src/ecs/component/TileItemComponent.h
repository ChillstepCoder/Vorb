#pragma once

#include "item/ItemStack.h"

// Tile items are owned by the Sim world and cannot be mutated except in result
// to sim item changes
class TileItemComponent {
    friend class IFullECS; // TODO: Something else own this? ItemEntitySystem?
public:
    TileItemComponent() = default;
    TileItemComponent(const ItemStack& itemStack, TileItemUID tileItemUID) :
        itemStack(itemStack),
        tileItemUID(tileItemUID) {
    }

    const ItemStack& getItemStack() const {
        return itemStack;
    }
    TileItemUID getTileItemUID() const {
        return tileItemUID;
    }
    void setTileItemUID(TileItemUID tileItemUID) {
        this->tileItemUID = tileItemUID;
    }

private:
    ItemStack itemStack;
    TileItemUID tileItemUID = INVALID_TILE_ITEM_UID;
};
static_assert(sizeof(TileItemComponent) == 24, "Keep small");

struct SimpleItemComponent {
    ItemStack itemStack;
};