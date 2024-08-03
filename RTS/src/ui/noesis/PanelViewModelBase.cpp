#include "stdafx.h"
#include "PanelViewModelBase.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>

NS_IMPLEMENT_REFLECTION(PanelViewModelBase) {
    NsProp("X", &PanelViewModelBase::GetX, &PanelViewModelBase::SetX);
    NsProp("Y", &PanelViewModelBase::GetY, &PanelViewModelBase::SetY);
    NsProp("MinWidth", &PanelViewModelBase::GetMinWidth, &PanelViewModelBase::SetMinWidth);
    NsProp("MinHeight", &PanelViewModelBase::GetMinHeight, &PanelViewModelBase::SetMinHeight);
    NsProp("MaxWidth", &PanelViewModelBase::GetMaxWidth, &PanelViewModelBase::SetMaxWidth);
    NsProp("MaxHeight", &PanelViewModelBase::GetMaxHeight, &PanelViewModelBase::SetMaxHeight);
    NsProp("AvailableWidth", &PanelViewModelBase::GetMaxWidth, &PanelViewModelBase::SetMaxWidth);
    NsProp("AvailableHeight", &PanelViewModelBase::GetMaxHeight, &PanelViewModelBase::SetMaxHeight);
    NsProp("CloseCommand", &PanelViewModelBase::GetCloseCommand);
    NsProp("WindowType", &PanelViewModelBase::GetWindowType);
}

PanelViewModelBase::PanelViewModelBase(GameUIPanel panel) : mPanel(panel) {
    mCloseCommand = Noesis::MakePtr<NoesisApp::DelegateCommand>([this](BaseComponent* param) {
        Close(param);
    });
}
