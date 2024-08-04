#include "stdafx.h"
#include "InventoryItemViewModel.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>

#include "input/InputDispatcher.h"

NS_IMPLEMENT_REFLECTION(InventoryItemViewModel) {
    NsProp(P_ITEM_TEXT, &InventoryItemViewModel::getText, &InventoryItemViewModel::setText);
    NsProp(P_ITEM_COUNT, &InventoryItemViewModel::getCount, &InventoryItemViewModel::setCount);
    NsProp(P_ITEM_ICON, &InventoryItemViewModel::getIcon, &InventoryItemViewModel::setIcon);
    NsProp("PickupCommand", &InventoryItemViewModel::GetPickupCommand);
    NsProp("PickupSingleCommand", &InventoryItemViewModel::GetPickupSingleCommand);
}

InventoryItemViewModel::InventoryItemViewModel(LocText itemName, ItemStack stack, TileItemUID uid, StrToken icon) : mUniqueId(uid), mText(itemName.c_str()), mStack(stack) {
    setIconFromAsset(icon);

    mPickupCommand = Noesis::MakePtr<NoesisApp::DelegateCommand>([this](BaseComponent* param) {
        if (!vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL) && !vui::InputDispatcher::key.isKeyPressed(VKEY_RCTRL)) {
            mPickupEvent(this, Noesis::EventArgs::Empty);
        }
    });

    mPickupSingleCommand = Noesis::MakePtr<NoesisApp::DelegateCommand>([this](BaseComponent* param) {
        mPickupSingleEvent(this, Noesis::EventArgs::Empty);
    });
}

const char* InventoryItemViewModel::getText() const {
    return mText.Str();
}

void InventoryItemViewModel::setText(const char* text) {
    if (mText != text) {
        mText = text;
        OnPropertyChanged(P_ITEM_TEXT);
    }
}

int InventoryItemViewModel::getCount() const {
    return mStack.count;
}

void InventoryItemViewModel::setCount(int val) {
    if (mStack.count != val) {
        mStack.count = val;
        OnPropertyChanged(P_ITEM_COUNT);
    }
}

const char* InventoryItemViewModel::getIcon() const {
    return mIcon.Str();
}

void InventoryItemViewModel::setIcon(const char* icon) {
    if (mIcon != icon) {
        mIcon = icon;
        OnPropertyChanged(P_ITEM_ICON);
    }
}

void InventoryItemViewModel::setIconFromAsset(StrToken iconAsset) {
    assert(iconAsset.isValid());
    char icon[256];
    ui32 length = 0;
    iconAsset.toString(icon, &length);
    icon[length++] = '.';
    icon[length++] = 'p';
    icon[length++] = 'n';
    icon[length++] = 'g';
    icon[length] = '\0';
    setIcon(icon);
}
