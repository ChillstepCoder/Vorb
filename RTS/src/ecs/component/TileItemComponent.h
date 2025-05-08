#pragma once

#include "item/ItemStack.h"

#include <NsCore/Delegate.h>

// Tile items are owned by the Sim world and cannot be mutated except in result
// to sim item changes
class TileItemComponent {
    friend class IFullECS; // TODO: Something else own this? ItemEntitySystem?
public:
    TileItemComponent() = default;
    TileItemComponent(ItemStack itemStack, TileItemUID tileItemUID) :
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
    // Usable padding here
    TileItemUID tileItemUID = INVALID_TILE_ITEM_UID;
};
static_assert(sizeof(TileItemComponent) == 24, "Keep small");

class TileItemContainerComponent {
    friend class IFullECS; // TODO: Something else own this? ItemEntitySystem?
public:
    TileItemContainerComponent() = default;
    TileItemContainerComponent(ItemStack itemStack, TileItemUID tileItemUID) :
        mItemStacks{ {itemStack, tileItemUID} } {
    }
    TileItemContainerComponent(const std::span<TileItemStack>& itemStacks) {
        mItemStacks.resize(itemStacks.size());
        for (size_t i = 0; i < itemStacks.size(); i++) {
            mItemStacks[i] = { itemStacks[i].toItemStack(), itemStacks[i].uniqueId };
        }
    }

    void setNewCount(TileItemUID tileItemUID, ui16 newCount) {
        for (ItemStackWithUID& stack : mItemStacks) {
            if (stack.tileItemUID == tileItemUID) {
                stack.itemStack.count = newCount;
                onChanged(*this);
                return;
            }
        }
        assert(false && "Item not found");
    }
    // Returns a stack with the taken count + remaining in pair
    std::pair<ItemStack, ui32 /*remaining*/> takeCount(TileItemUID tileItemUID, i32 count) {
        assert(count > 0);
        for (ItemStackWithUID& stack : mItemStacks) {
            if (stack.tileItemUID == tileItemUID) {
                assert(count <= stack.itemStack.count);
                stack.itemStack.count -= count;

                ItemStack takenStack = stack.itemStack;
                takenStack.count = count;

                if (stack.itemStack.count == 0) {
                    // Remove
                    stack = mItemStacks.back();
                    mItemStacks.pop_back();
                    onChanged(*this);
                    return std::make_pair(takenStack, 0);
                }
                onChanged(*this);
                return std::make_pair(takenStack, stack.itemStack.count);
            }
        }
        return std::make_pair(ItemStack(), 0);
    }

    bool isEmpty() const {
        return mItemStacks.empty();
    }

    const std::vector<ItemStackWithUID>& getItemStacks() const {
        return mItemStacks;
    }

    void addItemStack(ItemStack itemStack, TileItemUID tileItemUID) {
        mItemStacks.push_back({ itemStack, tileItemUID });
        onChanged(*this);
    }

    Noesis::Delegate<void(TileItemContainerComponent& cmp)> onChanged;
private:
    std::vector<ItemStackWithUID> mItemStacks;
};

struct SimpleItemComponent {
    ItemStack itemStack;
};
