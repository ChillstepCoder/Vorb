#pragma once

#include <NsApp/NotifyPropertyChangedBase.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/String.h>
#include <NsCore/ReflectionDeclareEnum.h>
#include <NsApp/DelegateCommand.h>

#include "item/ItemStack.h"

class InventoryItemViewModel : public NoesisApp::NotifyPropertyChangedBase {
public:
    InventoryItemViewModel(LocText itemName, ItemStack stack, TileItemUID uid, StrToken icon);

#define P_ITEM_TEXT "ItemText"
#define P_ITEM_COUNT "ItemCount"
#define P_ITEM_ICON "ItemIcon"

    const char* getText() const;
    void setText(const char* text);

    int getCount() const;
    void setCount(int val);

    const char* getIcon() const;
    void setIcon(const char* icon);
    void setIconFromAsset(StrToken iconAsset);

    ItemStack getItemStack() const {
        return mStack;
    }

    const NoesisApp::DelegateCommand* GetPickupCommand() const {
        return mPickupCommand;
    }

    Noesis::EventHandler& OnPickup() { return mPickupEvent; }

    NS_DECLARE_REFLECTION(InventoryItemViewModel, NoesisApp::NotifyPropertyChangedBase);
public:
    Noesis::Ptr<NoesisApp::DelegateCommand> mPickupCommand;
    Noesis::EventHandler mPickupEvent;

    TileItemUID mUniqueId;
private:
    // Properties
    Noesis::String mText;
    Noesis::String mIcon;
    ItemStack mStack;
};
