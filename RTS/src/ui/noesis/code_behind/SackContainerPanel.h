#pragma once

#include <NsGui/UserControl.h>

#include <NsGui/ScrollViewer.h>
#include <NsGui/ItemsControl.h>
#include <NsCore/Vector.h>
#include <NsCore/String.h>
#include <NsGui/ObservableCollection.h>
#include <NsGui/UIElementCollection.h>
#include <NsApp/NotifyPropertyChangedBase.h>

#include "item/ItemStack.h"

class InventoryItemDataModel : public NoesisApp::NotifyPropertyChangedBase {
public:
    InventoryItemDataModel(LocText itemName, int count, TileItemUID uid) : mUniqueId(uid), mText(itemName.c_str()), mCount(count) {}

    const char* getText() const { return mText.Str(); }
    void setText(const char* text) {
        if (mText != text) {
            mText = text;
            OnPropertyChanged("ItemText");
        }
    }
    int getCount() const { return mCount; }
    void setCount(int val) {
        if (mCount != val) {
            mCount = val;
            OnPropertyChanged("Count");
        }
    }

    NS_IMPLEMENT_INLINE_REFLECTION(InventoryItemDataModel, NoesisApp::NotifyPropertyChangedBase) {
        NsProp("ItemText", &InventoryItemDataModel::getText, &InventoryItemDataModel::setText);
        NsProp("ItemCount", &InventoryItemDataModel::getCount, &InventoryItemDataModel::setCount);
    }

public:
    TileItemUID mUniqueId;
private:
    // Properties
    Noesis::String mText;
    int mCount;
};

class ItemDetailsBar;

class SackContainerPanel : public Noesis::UserControl
{
public:
    SackContainerPanel();

    static void RegisterChildren();
    void reset();
    void updateItems(std::vector<ItemStackWithUID> items);

private:
    void InitializeComponent();
    bool ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler) override;

    void OnScrollViewerPreviewMouseWheel(Noesis::BaseComponent* sender, const Noesis::MouseWheelEventArgs& e);
    void OnScrollViewerScrollChanged(Noesis::BaseComponent* sender, const Noesis::ScrollChangedEventArgs& e);
    void OnScrollViewerMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e);
    void OnInventoryButtonMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e);
    void OnInventoryButtonMouseLeave(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e);
    void LoadMoreItems(int count);

    Noesis::ScrollViewer* mScrollViewer;
    Noesis::ItemsControl* mInventoryItemsControl;
    ItemDetailsBar* mItemDetailsBar;
    Noesis::Ptr<Noesis::ObservableCollection<InventoryItemDataModel>> mInventoryItems;
    int mTotalItems;

    NS_IMPLEMENT_INLINE_REFLECTION_(SackContainerPanel, UserControl, "AM.SackContainer")
};

