#include "stdafx.h"
#include "NoesisPanelManager.h"

#include <NsCore/ReflectionImplementEnum.h>
#include <NsCore/RegisterComponent.h>
#include <NsGui/DataTemplate.h>
#include <NsGui/DataTemplateSelector.h>

#include "ui/noesis/code_behind/SackContainerPanel.h"


NoesisPanelManager* sInstance = nullptr;

NS_IMPLEMENT_REFLECTION(NoesisPanelManager, "AM.PanelManager") {
    NsProp("ActiveWindows", &NoesisPanelManager::GetActiveWindows);
}

NoesisPanelManager::NoesisPanelManager() {
    mActiveWindows = *new Noesis::ObservableCollection<PanelViewModelBase>();
    
    // TEST
    //mPanelWantsActive[e_cast(GameUIPanel::SackContainer)].store(1);

    sInstance = this;
}

NoesisPanelManager::~NoesisPanelManager() {
    assert(sInstance == this);
    sInstance = nullptr;
}

NoesisPanelManager* NoesisPanelManager::GetInstance() {
    return sInstance;
}

bool NoesisPanelManager::update() {
    bool hasAnyWindow = false;
    for (i32 i = 0; i < e_count(GameUIPanel); ++i) {
        GameUIPanel uiPanel = (GameUIPanel)i;
        hasAnyWindow |= UpdatePanel(uiPanel);
    }
    return hasAnyWindow;
}

Noesis::ObservableCollection<PanelViewModelBase>* NoesisPanelManager::GetActiveWindows() const {
    return mActiveWindows;
}

void NoesisPanelManager::AddWindow(GameUIPanel panel) {
    mPanelWantsActive[e_cast(panel)].store(1);
    // Render thread immediately updates
    if (IS_RENDER_THREAD()) {
        UpdatePanel(panel);
    }
}

void NoesisPanelManager::RemoveWindow(GameUIPanel panel) {
    mPanelWantsActive[e_cast(panel)].store(0);
    // Render thread immediately updates
    if (IS_RENDER_THREAD()) {
        UpdatePanel(panel);
    }
}

void NoesisPanelManager::ToggleWindow(GameUIPanel panel) {
    mPanelWantsActive[e_cast(panel)].fetch_xor(1, std::memory_order_relaxed) ^ 1;
    // Render thread immediately updates
    if (IS_RENDER_THREAD()) {
        UpdatePanel(panel);
    }
}

bool NoesisPanelManager::UpdatePanel(GameUIPanel panel) {
    const int i = e_cast(panel);
    // Only check the atomic once per frame
    if (mPanelWantsActive[i]) {
        if (!mPanelWasActive[i]) {
            PanelViewModelBase* newWindow;
            switch (panel) {
                case GameUIPanel::Inventory:
                    return false;
                    //panic("IMPLEMENT INVENTORY");
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
            PanelViewModelBase* window = mActiveWindows->Get(i);
            if (window->GetWindowType() == panel) {
                RemoveWindowInternal(window);
                break;
            }
        }
    }
    return false;
}

void NoesisPanelManager::AddWindowInternal(PanelViewModelBase* window) {
    mActiveWindows->Add(window);
    window->CloseRequested() += MakeDelegate(this, &NoesisPanelManager::OnWindowCloseRequested);
}

void NoesisPanelManager::RemoveWindowInternal(PanelViewModelBase* window) {
    window->CloseRequested() -= MakeDelegate(this, &NoesisPanelManager::OnWindowCloseRequested);
    mActiveWindows->Remove(window);
}

SackContainerViewModel* NoesisPanelManager::GetSackContainerViewModel() const {
    for (int i = 0; i < mActiveWindows->Count(); i++) {
        SackContainerViewModel* sackContainer = Noesis::DynamicCast<SackContainerViewModel*>(mActiveWindows->Get(i));
        if (sackContainer) {
            return sackContainer;
        }
    }
    return nullptr;
}

void NoesisPanelManager::OnWindowCloseRequested(BaseComponent* sender, const Noesis::EventArgs&) {
    PanelViewModelBase* window = static_cast<PanelViewModelBase*>(sender);
    RemoveWindow(window->GetWindowType());
}