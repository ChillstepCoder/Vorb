#include "stdafx.h"
#include "PanelViewModelBase.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>

NS_IMPLEMENT_REFLECTION(PanelViewModelBase) {
    IMPLEMENT_PANEL_BASE_REFLECTION(PanelViewModelBase);
}

PanelViewModelBase::PanelViewModelBase(GameUIPanel panel) : mPanel(panel) {
    mCloseCommand = Noesis::MakePtr<NoesisApp::DelegateCommand>([this](BaseComponent* param) {
        Close(param);
    });
}
