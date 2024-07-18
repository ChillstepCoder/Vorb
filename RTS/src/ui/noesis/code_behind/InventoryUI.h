#pragma once

#include <NsGui/UserControl.h>

#include <NsGui/ScrollViewer.h>
#include <NsGui/Popup.h>
#include <NsGui/ItemsControl.h>
#include <NsCore/Vector.h>
#include <NsGui/ObservableCollection.h>
#include <NsGui/UIElementCollection.h>

class InventoryItem : public Noesis::BaseComponent
{
public:
    InventoryItem() {}
    NS_IMPLEMENT_INLINE_REFLECTION_(InventoryItem, Noesis::BaseComponent)
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
    Noesis::Ptr<Noesis::ObservableCollection<InventoryItem>> _inventoryItems;
    int _totalItems;
    CustomPopup* mActivePopupBar = nullptr;

    NS_IMPLEMENT_INLINE_REFLECTION_(InventoryUI, UserControl, "AM.Inventory")
};

