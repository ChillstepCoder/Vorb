#pragma once

#include <NsGui/UserControl.h>

#include <NsGui/ScrollViewer.h>
#include <NsGui/ItemsControl.h>
#include <NsCore/Vector.h>
#include <NsCore/String.h>
#include <NsGui/ObservableCollection.h>
#include <NsGui/UIElementCollection.h>
#include <NsApp/NotifyPropertyChangedBase.h>


class InventoryItemDataModel : public NoesisApp::NotifyPropertyChangedBase
{
public:
    InventoryItemDataModel() : mText("Default Item Text") {}

    const char* getText() const { return mText.Str(); }
    void setText(const char* text) {
        if (mText != text) {
            mText = text;
            OnPropertyChanged("ItemText");
        }
    }

    NS_IMPLEMENT_INLINE_REFLECTION(InventoryItemDataModel, NoesisApp::NotifyPropertyChangedBase) {
        NsProp("ItemText", &InventoryItemDataModel::getText, &InventoryItemDataModel::setText);
    }

private:
    Noesis::String mText;
};


class CustomPopup;

class InventoryUI : public Noesis::UserControl
{
public:
    InventoryUI();

    static void RegisterChildren();

private:
    void InitializeComponent();
    bool ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler) override;

    void OnScrollViewerPreviewMouseWheel(Noesis::BaseComponent* sender, const Noesis::MouseWheelEventArgs& e);
    void OnScrollViewerScrollChanged(Noesis::BaseComponent* sender, const Noesis::ScrollChangedEventArgs& e);
    void OnScrollViewerMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e);
    void OnInventoryButtonMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e);
    void OnInventoryButtonMouseLeave(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e);
    void LoadMoreItems(int count);

    Noesis::ScrollViewer* _scrollViewer;
    Noesis::ItemsControl* _inventoryItemsControl;
    Noesis::Ptr<Noesis::ObservableCollection<InventoryItemDataModel>> _inventoryItems;
    int _totalItems;
    CustomPopup* mActivePopupBar = nullptr;

    NS_IMPLEMENT_INLINE_REFLECTION_(InventoryUI, UserControl, "AM.Inventory")
};

