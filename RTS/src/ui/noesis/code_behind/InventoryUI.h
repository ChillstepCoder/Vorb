#pragma once

#include <NsGui/UserControl.h>

class InventoryUI : public Noesis::UserControl
{
public:
    InventoryUI();

private:
    void initializeComponent();
    bool ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler) override;

    // Events
    void OnButton1Click(Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& args)
    {
        LOG_CRITICAL("Button1 was clicked");
    }

    NS_IMPLEMENT_INLINE_REFLECTION_(InventoryUI, UserControl, "AM.Inventory")
};

