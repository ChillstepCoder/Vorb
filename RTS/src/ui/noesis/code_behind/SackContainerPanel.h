#pragma once

#include <NsGui/UserControl.h>

#include <NsGui/ScrollViewer.h>
#include <NsGui/ItemsControl.h>
#include <NsCore/Vector.h>
#include <NsCore/String.h>
#include <NsGui/ObservableCollection.h>
#include <NsGui/UIElementCollection.h>
#include <NsApp/NotifyPropertyChangedBase.h>
#include <NsApp/DelegateCommand.h>

#include "item/ItemStack.h"
#include "ui/noesis/WindowViewModelBase.h"
#include "ui/noesis/inventory/InventoryItemViewModel.h"

class ItemDetailsBar;

class SackContainerViewModel : public WindowViewModelBase {
public:
    SackContainerViewModel();

    // Expose the ObservableCollection
    Noesis::ObservableCollection<InventoryItemViewModel>* GetInventoryItems() const {
        return mInventoryItems;
    }

    void updateItems(std::vector<ItemStackWithUID> items);

    Noesis::EventHandler& CloseRequested() { return mCloseRequested; }

private:
    void Close(BaseComponent* param) {
        mCloseRequested(this, Noesis::EventArgs::Empty);
    }

    Noesis::EventHandler mCloseRequested;

    Noesis::Ptr<Noesis::ObservableCollection<InventoryItemViewModel>> mInventoryItems;

    NS_DECLARE_REFLECTION(SackContainerViewModel, NoesisApp::NotifyPropertyChangedBase)
};

class SackContainerPanel : public Noesis::UserControl
{
public:
    SackContainerPanel();

    static void RegisterChildren();

private:
    void InitializeComponent();
    void PrintVisualTree(Noesis::Visual* element, int depth);
    void OnInit() override;
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
    Noesis::Ptr<Noesis::ObservableCollection<InventoryItemViewModel>> mInventoryItems;
    int mTotalItems;

    NS_DECLARE_REFLECTION(SackContainerPanel, UserControl, "AM.SackContainerPanel")
};

