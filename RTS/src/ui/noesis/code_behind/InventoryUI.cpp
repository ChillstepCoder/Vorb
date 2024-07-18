#include "stdafx.h"
#include "InventoryUI.h"

#include <NsCore/RegisterComponent.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/Button.h>
#include <NsGui/Uri.h>
#include <NsGui/VisualTreeHelper.h>
#include <NsGui/RoutedEvent.h>


class CustomPopup : public Noesis::Popup {
public:
    CustomPopup() {
        SetPlacement(Noesis::PlacementMode_AbsolutePoint);
    }

    void Init(f32 x, Noesis::Button* attachedButton) {
        mAttachedButton = attachedButton;
        mScreenX = x;
        UpdatePopupPosition();
    }

    void OnScroll() {
        UpdatePopupPosition();
    }

protected:
    void OnChildDesiredSizeChanged(Noesis::UIElement* child) override {
        Noesis::Popup::OnChildDesiredSizeChanged(child);
        UpdatePopupPosition();
    }

private:
    void UpdatePopupPosition() {
        if (mAttachedButton) {
            SetHorizontalOffset(mScreenX);
            Noesis::Point buttonBottomLeft = mAttachedButton->PointToScreen(Noesis::Point(0, mAttachedButton->GetActualHeight()));
            SetVerticalOffset(buttonBottomLeft.y - 4);
        }
    }

    f32 mScreenX = 0.0f;
    Noesis::Button* mAttachedButton = nullptr;

    NS_IMPLEMENT_INLINE_REFLECTION_(CustomPopup, Noesis::Popup, "AM.CustomPopup")
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

    _inventoryItems = *new Noesis::ObservableCollection<InventoryItem>();
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
    CustomPopup* popup = parent->FindName<CustomPopup>("ItemPopup");

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
        _inventoryItems->Add(Noesis::MakePtr<InventoryItem>());
    }
}
