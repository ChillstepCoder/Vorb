#include "stdafx.h"
#include "InventoryUI.h"

#include <NsGui/IntegrationAPI.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/Button.h>
#include <NsGui/Uri.h>


InventoryUI::InventoryUI() {
    initializeComponent();
}

void InventoryUI::initializeComponent() {
    Noesis::GUI::LoadComponent(this, Noesis::Uri("inventory.xaml"));
}

bool InventoryUI::ConnectEvent(Noesis::BaseComponent* source, const char* event, const char* handler) {
    NS_CONNECT_EVENT(Noesis::Button, Click, OnButton1Click);
}
