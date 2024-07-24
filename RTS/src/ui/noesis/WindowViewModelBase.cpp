#include "stdafx.h"
#include "WindowViewModelBase.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>

NS_IMPLEMENT_REFLECTION(WindowViewModelBase) {
    IMPLEMENT_WINDOW_BASE_REFLECTION(WindowViewModelBase);
}

WindowViewModelBase::WindowViewModelBase(GameUIPanel panel) : mPanel(panel) {
    mCloseCommand = Noesis::MakePtr<NoesisApp::DelegateCommand>([this](BaseComponent* param) {
        Close(param);
    });
}
