#include "stdafx.h"
#include "NoesisWindowManager.h"

#include <NsCore/ReflectionImplementEnum.h>
#include <NsCore/RegisterComponent.h>
#include <NsGui/DataTemplate.h>
#include <NsGui/DataTemplateSelector.h>

#include "ui/noesis/code_behind/SackContainerPanel.h"


NoesisWindowManager* sInstance = nullptr;

NS_IMPLEMENT_REFLECTION(NoesisWindowManager, "AM.NoesisWindowManager") {
    NsProp("ActiveWindows", &NoesisWindowManager::GetActiveWindows);
}

NoesisWindowManager::NoesisWindowManager() {
    mActiveWindows = *new Noesis::ObservableCollection<WindowViewModelBase>();
    
    // TEST
    //mPanelWantsActive[e_cast(GameUIPanel::SackContainer)].store(1);

    sInstance = this;
}

NoesisWindowManager::~NoesisWindowManager() {
    assert(sInstance == this);
    sInstance = nullptr;
}

NoesisWindowManager* NoesisWindowManager::GetInstance() {
    return sInstance;
}

bool NoesisWindowManager::update() {
    bool hasAnyWindow = false;
    for (i32 i = 0; i < e_count(GameUIPanel); ++i) {
        GameUIPanel uiPanel = (GameUIPanel)i;
        hasAnyWindow |= UpdatePanel(uiPanel);
    }
    return hasAnyWindow;
}

Noesis::ObservableCollection<WindowViewModelBase>* NoesisWindowManager::GetActiveWindows() const {
    return mActiveWindows;
}

void NoesisWindowManager::AddWindow(GameUIPanel panel) {
    mPanelWantsActive[e_cast(panel)].store(1);
    // Render thread immediately updates
    if (IS_RENDER_THREAD()) {
        UpdatePanel(panel);
    }
}

void NoesisWindowManager::RemoveWindow(GameUIPanel panel) {
    mPanelWantsActive[e_cast(panel)].store(0);
    // Render thread immediately updates
    if (IS_RENDER_THREAD()) {
        UpdatePanel(panel);
    }
}

void NoesisWindowManager::ToggleWindow(GameUIPanel panel) {
    mPanelWantsActive[e_cast(panel)].fetch_xor(1, std::memory_order_relaxed) ^ 1;
    // Render thread immediately updates
    if (IS_RENDER_THREAD()) {
        UpdatePanel(panel);
    }
}

bool NoesisWindowManager::UpdatePanel(GameUIPanel panel) {
    const int i = e_cast(panel);
    // Only check the atomic once per frame
    if (mPanelWantsActive[i]) {
        if (!mPanelWasActive[i]) {
            WindowViewModelBase* newWindow;
            switch (panel) {
                case GameUIPanel::Inventory:
                    panic("IMPLEMENT INVENTORY");
                    break;
                case GameUIPanel::SackContainer:
                    newWindow = new SackContainerViewModel();
                    break;
                default:
                    panic("Unhandled UI panel {} in NoesisWindowManager::update", (int)panel);
                    break;

            }
            static_assert(e_count(GameUIPanel) == 2);
            AddWindowInternal(newWindow);
            mPanelWasActive[i] = true;
        }
        return true;
    }
    else if (mPanelWasActive[i]) {
        // Release input focus
        mPanelWasActive[i] = false;
        for (int i = 0; i < mActiveWindows->Count(); i++) {
            WindowViewModelBase* window = mActiveWindows->Get(i);
            if (window->GetWindowType() == panel) {
                RemoveWindowInternal(window);
                break;
            }
        }
    }
    return false;
}

void NoesisWindowManager::AddWindowInternal(WindowViewModelBase* window) {
    mActiveWindows->Add(window);
    window->CloseRequested() += MakeDelegate(this, &NoesisWindowManager::OnWindowCloseRequested);
}

void NoesisWindowManager::RemoveWindowInternal(WindowViewModelBase* window) {
    window->CloseRequested() -= MakeDelegate(this, &NoesisWindowManager::OnWindowCloseRequested);
    mActiveWindows->Remove(window);
}

SackContainerViewModel* NoesisWindowManager::GetSackContainerViewModel() const {
    for (int i = 0; i < mActiveWindows->Count(); i++) {
        SackContainerViewModel* sackContainer = Noesis::DynamicCast<SackContainerViewModel*>(mActiveWindows->Get(i));
        if (sackContainer) {
            return sackContainer;
        }
    }
    return nullptr;
}

void NoesisWindowManager::OnWindowCloseRequested(BaseComponent* sender, const Noesis::EventArgs&) {
    WindowViewModelBase* window = static_cast<WindowViewModelBase*>(sender);
    RemoveWindow(window->GetWindowType());
}