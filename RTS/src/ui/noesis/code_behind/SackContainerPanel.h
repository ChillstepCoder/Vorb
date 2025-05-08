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
#include "ui/noesis/PanelViewModelBase.h"
#include "ui/noesis/inventory/InventoryItemViewModel.h"

#include "ecs/component/ThreadSharedComponent.h"

class ItemDetailsBar;

class SackContainerViewModel : public PanelViewModelBase {
public:
    SackContainerViewModel();

    // Expose the ObservableCollection
    Noesis::ObservableCollection<InventoryItemViewModel>* GetInventoryItems() const {
        return mInventoryItems;
    }

    void updateItems(RenderThreadSharedComponentDataPtr& data, std::vector<ItemStackWithUID> items);

    f32v3 getWorldPosition();

private:
    void addItemInternal(Noesis::Ptr<InventoryItemViewModel> itemModel);
    void onClose() override;

    Noesis::Ptr<Noesis::ObservableCollection<InventoryItemViewModel>> mInventoryItems;

    NS_DECLARE_REFLECTION(SackContainerViewModel, PanelViewModelBase)

    RenderThreadSharedComponentDataPtr mSharedData;
};

class SackContainerPanel : public Noesis::UserControl
{
public:
    SackContainerPanel();
    ~SackContainerPanel();

    static void RegisterChildren();

private:
    void InitializeComponent();
    void OnInit() override;
    bool ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler) override;

    void OnScrollViewerPreviewMouseWheel(Noesis::BaseComponent* sender, const Noesis::MouseWheelEventArgs& e);
    void OnScrollViewerScrollChanged(Noesis::BaseComponent* sender, const Noesis::ScrollChangedEventArgs& e);
    void OnInventoryButtonMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e);
    void OnInventoryButtonMouseLeave(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e);
    void OnMouseDownRight(Noesis::BaseComponent* sender, const Noesis::MouseButtonEventArgs& e);

    Noesis::ScrollViewer* mScrollViewer;
    ItemDetailsBar* mItemDetailsBar;

    NS_DECLARE_REFLECTION(SackContainerPanel, UserControl, "AM.SackContainerPanel")
};

