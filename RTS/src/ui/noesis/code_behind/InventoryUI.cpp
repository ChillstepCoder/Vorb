#include "stdafx.h"
#include "InventoryUI.h"

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


class CustomPopup : public Noesis::UserControl {
public:
    typedef Noesis::Delegate<void(BaseComponent*, const Noesis::RoutedEventArgs&)> ItemPopupHandler;

    CustomPopup() {
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

    NS_IMPLEMENT_INLINE_REFLECTION(CustomPopup, Noesis::UserControl, "AM.CustomPopup") {
        Noesis::UIElementData* data = NsMeta<Noesis::UIElementData>(Noesis::TypeOf<SelfClass>());
        data->RegisterEvent(PopupOpenedEvent, "PopupOpened", Noesis::RoutingStrategy_Bubble);
        data->RegisterEvent(PopupClosedEvent, "PopupClosed", Noesis::RoutingStrategy_Bubble);
        data->RegisterProperty<Noesis::String>(ItemTextProperty, "ItemText", Noesis::PropertyMetadata::Create(Noesis::String("Hello2")));
        data->RegisterProperty<bool>(IsOpenProperty, "IsOpen", Noesis::PropertyMetadata::Create(false));
    }
};


InventoryUI::InventoryUI() {
    InitializeComponent();


    _totalItems = 1000;
    LoadMoreItems(16);
}

void InventoryUI::RegisterChildren() {
    Noesis::RegisterComponent<CustomPopup>();
}

void InventoryUI::InitializeComponent() {
    ASSERT_RENDER_THREAD();
    Noesis::GUI::LoadComponent(this, Noesis::Uri("inventory.xaml"));

    _scrollViewer = FindName<Noesis::ScrollViewer>("ScrollViewer");
    _inventoryItemsControl = FindName<Noesis::ItemsControl>("InventoryItemsControl");

    _inventoryItems = *new Noesis::ObservableCollection<InventoryItemDataModel>();
    _inventoryItemsControl->SetItemsSource(_inventoryItems);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool InventoryUI::ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler)
{
    NS_CONNECT_EVENT(Noesis::ScrollViewer, PreviewMouseWheel, OnScrollViewerPreviewMouseWheel);
    NS_CONNECT_EVENT(Noesis::ScrollViewer, ScrollChanged, OnScrollViewerScrollChanged);
    NS_CONNECT_EVENT(Noesis::ScrollViewer, MouseEnter, OnScrollViewerMouseEnter);
    NS_CONNECT_EVENT(Noesis::Button, MouseEnter, OnInventoryButtonMouseEnter);
    NS_CONNECT_EVENT(Noesis::Button, MouseLeave, OnInventoryButtonMouseLeave);
    NS_CONNECT_EVENT(Noesis::ScrollViewer, ScrollChanged, OnScrollViewerScrollChanged);
    return UserControl::ConnectEvent(source, event, handler);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InventoryUI::OnScrollViewerPreviewMouseWheel(Noesis::BaseComponent* sender, const Noesis::MouseWheelEventArgs& e) {
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
        LOG_DEBUG("LOAD_MORE");
    }
}

void InventoryUI::OnScrollViewerScrollChanged(Noesis::BaseComponent* sender, const Noesis::ScrollChangedEventArgs& e) {
    if (mActivePopupBar) {
        mActivePopupBar->OnScroll();
    }
}

void InventoryUI::OnScrollViewerMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e)
{
    Noesis::ScrollViewer* scrollViewer = static_cast<Noesis::ScrollViewer*>(sender);
    scrollViewer->Focus();
}

void InventoryUI::OnInventoryButtonMouseEnter(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e) {
    Noesis::Button* button = static_cast<Noesis::Button*>(sender);
    Noesis::FrameworkElement* parent = static_cast<Noesis::FrameworkElement*>(button->GetParent());
    CustomPopup* popup = parent->FindName<CustomPopup>("SharedItemPopup");

    if (mActivePopupBar) {
        mActivePopupBar->SetIsOpen(false);
    }

    if (popup) {
        // Get the View
        Noesis::Size size = _scrollViewer->GetRenderSize();
        Noesis::Point scrollRoot = _scrollViewer->PointToScreen(Noesis::Point(0, 0));
        // Get the size of the rendering surface
        uint32_t width, height;
        Noesis::Point screenCenter(size.width / 2.0f, size.height / 2.0f);

        // Get button position
        popup->SetIsOpen(true);
        popup->Init(scrollRoot.x + 12, button);
    }
    mActivePopupBar = popup;
}

void InventoryUI::OnInventoryButtonMouseLeave(Noesis::BaseComponent* sender, const Noesis::MouseEventArgs& e) {
    if (mActivePopupBar) {
        mActivePopupBar->SetIsOpen(false);
        mActivePopupBar = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InventoryUI::LoadMoreItems(int count)
{
    for (int i = 0; i < count && _inventoryItems->Count() < _totalItems; ++i)
    {
        _inventoryItems->Add(Noesis::MakePtr<InventoryItemDataModel>());
    }
}