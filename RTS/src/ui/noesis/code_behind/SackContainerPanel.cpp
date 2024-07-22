#include "stdafx.h"
#include "SackContainerPanel.h"

#include <NsCore/RegisterComponent.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/Button.h>
#include <NsGui/Canvas.h>
#include <NsGui/Uri.h>
#include <NsGui/VisualTreeHelper.h>
#include <NsGui/RoutedEvent.h>
#include <NsGui/UIElementData.h>
#include <NsGui/INotifyPropertyChanged.h>
#include <NsGui/UserControl.h>

#include "item/ItemRepository.h"

class ItemDetailsBar : public Noesis::UserControl {
public:
    typedef Noesis::Delegate<void(BaseComponent*, const Noesis::RoutedEventArgs&)> ItemPopupHandler;

    ItemDetailsBar() {
         SetVisibility(Noesis::Visibility_Hidden);
    }

    void Init(f32 x, Noesis::Button* attachedButton) {
        mAttachedButton = attachedButton;
        mScreenX = x;
        UpdatePopupPosition();
    }

    void OnScroll() {
        UpdatePopupPosition();
    }

    UIElement::RoutedEvent_<ItemPopupHandler> PopupOpened() {
        return UIElement::RoutedEvent_<ItemPopupHandler>(this, PopupOpenedEvent);
    }
    UIElement::RoutedEvent_<ItemPopupHandler> PopupClosed() {
        return UIElement::RoutedEvent_<ItemPopupHandler>(this, PopupClosedEvent);
    }

    const char* GetItemText() const {
        return Noesis::DependencyObject::GetValue<Noesis::String>(ItemTextProperty).Str();
    }
    void SetItemText(const char* text) {
        Noesis::DependencyObject::SetValue<Noesis::String>(ItemTextProperty, text);
    }

    void SetIsOpen(bool isOpen) {
        if (isOpen == GetIsOpen()) return;
        SetValue<bool>(IsOpenProperty, isOpen);
        OnIsOpenChanged(isOpen);
    }
    bool GetIsOpen() const {
        return GetValue<bool>(IsOpenProperty);
    }
    /// Dependency properties and routed events
    //@{
    inline static const Noesis::DependencyProperty* ItemTextProperty;
    inline static const Noesis::DependencyProperty* IsOpenProperty;
    inline static const Noesis::RoutedEvent* PopupOpenedEvent;
    inline static const Noesis::RoutedEvent* PopupClosedEvent;
    //@}

protected:
    void OnChildDesiredSizeChanged(Noesis::UIElement* child) override {
        UpdatePopupPosition();
    }

private:
    void UpdatePopupPosition() {
        if (mAttachedButton) {
            Noesis::Visual* parent = GetParent();
            if (Noesis::Canvas* canvas = Noesis::DynamicCast<Noesis::Canvas*>(parent)) {
                Noesis::Point pos = mAttachedButton->TranslatePoint(Noesis::Point(0, mAttachedButton->GetActualHeight()), canvas);
               // Noesis::Canvas::SetLeft(this, pos.x);
                Noesis::Canvas::SetTop(this, pos.y - 4);
            }
        }
    }
    void OnIsOpenChanged(bool newValue) {
        SetVisibility(newValue ? Noesis::Visibility_Visible : Noesis::Visibility_Hidden);
        Noesis::RoutedEventArgs args(this, newValue ? PopupOpenedEvent : PopupClosedEvent);
        RaiseEvent(args);
    }

    f32 mScreenX = 0.0f;
    Noesis::Button* mAttachedButton = nullptr;

    NS_IMPLEMENT_INLINE_REFLECTION(ItemDetailsBar, Noesis::UserControl, "AM.ItemDetailsBar") {
        Noesis::UIElementData* data = NsMeta<Noesis::UIElementData>(Noesis::TypeOf<SelfClass>());
        data->RegisterEvent(PopupOpenedEvent, "Opened", Noesis::RoutingStrategy_Bubble);
        data->RegisterEvent(PopupClosedEvent, "Closed", Noesis::RoutingStrategy_Bubble);
        data->RegisterProperty<Noesis::String>(ItemTextProperty, "ItemText", Noesis::PropertyMetadata::Create(Noesis::String("Hello2")));
        data->RegisterProperty<bool>(IsOpenProperty, "IsOpen", Noesis::PropertyMetadata::Create(false));
    }
};


SackContainerPanel::SackContainerPanel() {
    InitializeComponent();

    mTotalItems = 128;
    LoadMoreItems(20);
}

void SackContainerPanel::RegisterChildren() {
    Noesis::RegisterComponent<ItemDetailsBar>();
}

void SackContainerPanel::reset() {
    mInventoryItems->Clear();
    mScrollViewer->ScrollToTop();
    mItemDetailsBar->SetIsOpen(false);
    mWantsClose = false;
}

void SackContainerPanel::updateItems(std::vector<ItemStackWithUID> items) {
    PROFILE_FUNCTION();
    ASSERT_RENDER_THREAD();
    // Remove any items that are no longer in the list and update existing counts
    for (int i = mInventoryItems->Count() - 1; i >= 0; --i) {
        for (int j = items.size() - 1; j >= 0; --j) {
            InventoryItemDataModel* model = mInventoryItems->Get(i);
            if (model->mUniqueId == items[j].tileItemUID) {
                model->setCount(items[j].itemStack.count);
                items.erase(items.begin() + j); // Remove from list (so we don't add it again later;
                break;
            }
            if (j == 0) {
                mInventoryItems->RemoveAt(i);
            }
        }
    }

    // Add any new items
    for (int i = 0; i < items.size(); ++i) {
        ItemStackWithUID& stack = items[i];
        const ItemDef& itemDef = ItemRepository::get().getLoadedOrUnloadedAsset(stack.itemStack.id);
        mInventoryItems->Add(Noesis::MakePtr<InventoryItemDataModel>(itemDef.mDisplayName, stack.itemStack.count, stack.tileItemUID, itemDef.mIconTextureRef.getAssetName()));
    }

    //// Add some empty ones at the end
    //for (int i = 0; i < 20; ++i) {
    //    mInventoryItems->Add(Noesis::MakePtr<InventoryItemDataModel>("Empty", 0, UINT32_MAX));
    //}
}

void SackContainerPanel::InitializeComponent() {
    ASSERT_RENDER_THREAD();
    Noesis::GUI::LoadComponent(this, Noesis::Uri("inventory.xaml"));

    mScrollViewer = FindName<Noesis::ScrollViewer>("ScrollViewer");
    mInventoryItemsControl = FindName<Noesis::ItemsControl>("InventoryItemsControl");

    mInventoryItems = *new Noesis::ObservableCollection<InventoryItemDataModel>();
    mInventoryItemsControl->SetItemsSource(mInventoryItems);

    mItemDetailsBar = FindName<ItemDetailsBar>("SharedItemDetailsBar");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SackContainerPanel::ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler) {
    NS_CONNECT_EVENT(Noesis::ScrollViewer, PreviewMouseWheel, OnScrollViewerPreviewMouseWheel);
    NS_CONNECT_EVENT(Noesis::ScrollViewer, ScrollChanged, OnScrollViewerScrollChanged);
    NS_CONNECT_EVENT(Noesis::ScrollViewer, MouseEnter, OnScrollViewerMouseEnter);
    NS_CONNECT_EVENT(Noesis::Button, MouseEnter, OnInventoryButtonMouseEnter);
    NS_CONNECT_EVENT(Noesis::Button, MouseLeave, OnInventoryButtonMouseLeave);
    NS_CONNECT_EVENT(Noesis::Button, Click, OnCloseButtonClick);
    NS_CONNECT_EVENT(Noesis::ScrollViewer, ScrollChanged, OnScrollViewerScrollChanged);
    return UserControl::ConnectEvent(source, event, handler);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SackContainerPanel::OnScrollViewerPreviewMouseWheel(Noesis::BaseComponent* sender, const Noesis::MouseWheelEventArgs& e) {
    Noesis::ScrollViewer* scrollViewer = static_cast<Noesis::ScrollViewer*>(sender);

    const f32 offsetDistance = e.wheelRotation * 20.0f;

    // Calculate new offset
    float newOffset = scrollViewer->GetVerticalOffset() - offsetDistance;
    // Clamp the offset to valid range
    newOffset = glm::clamp(newOffset, 0.0f, scrollViewer->GetScrollableHeight());

    scrollViewer->ScrollToVerticalOffset(newOffset);
    e.handled = true;

    // Check if we're near the bottom and load more items if needed
    if (scrollViewer->GetVerticalOffset() >= scrollViewer->GetScrollableHeight() - 100)
    {
        LoadMoreItems(4); // Load 4 more items (1 row) when near the bottom
    }
}

void SackContainerPanel::OnScrollViewerScrollChanged(Noesis::BaseComponent* sender, const Noesis::ScrollChangedEventArgs& e) {
    if (mItemDetailsBar->GetIsOpen()) {
        mItemDetailsBar->OnScroll();
    }
}

void SackContainerPanel::OnScrollViewerMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e)
{
    Noesis::ScrollViewer* scrollViewer = static_cast<Noesis::ScrollViewer*>(sender);
    scrollViewer->Focus();
}

void SackContainerPanel::OnInventoryButtonMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e) {
    Noesis::Button* button = static_cast<Noesis::Button*>(sender);
    Noesis::FrameworkElement* parent = static_cast<Noesis::FrameworkElement*>(button->GetParent());

    // Get the DataContext of the parent Grid
    InventoryItemDataModel* itemData = Noesis::DynamicCast<InventoryItemDataModel*>(parent->GetDataContext());

    // Get the View
    Noesis::Size size = mScrollViewer->GetRenderSize();
    Noesis::Point scrollRoot = mScrollViewer->PointToScreen(Noesis::Point(0, 0));
    // Get the size of the rendering surface
    uint32_t width, height;
    Noesis::Point screenCenter(size.width / 2.0f, size.height / 2.0f);

    // Get button position
    mItemDetailsBar->SetIsOpen(true);
    mItemDetailsBar->Init(scrollRoot.x + 12, button);
    mItemDetailsBar->SetItemText(itemData->getText());
}

void SackContainerPanel::OnInventoryButtonMouseLeave(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e) {
    if (mItemDetailsBar->GetIsOpen()) {
        mItemDetailsBar->SetIsOpen(false);
    }
}

void SackContainerPanel::OnCloseButtonClick(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args) {
    mWantsClose = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SackContainerPanel::LoadMoreItems(int count)
{
    // No longer
    /*  for (int i = 0; i < count && mInventoryItems->Count() < mTotalItems; ++i)
      {
          mInventoryItems->Add(Noesis::MakePtr<InventoryItemDataModel>());
      }*/
}