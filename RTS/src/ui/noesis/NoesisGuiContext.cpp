#include "stdafx.h"
#include "NoesisGuiContext.h"

#include <SDL2/SDL_events.h>
#include <NsGui/IView.h>

#include <NsCore/Noesis.h>
#include <NsCore/ReflectionImplementEmpty.h>
#include <NsCore/RegisterComponent.h>
#include <NsGui/Button.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/TextBlock.h>
#include <NsGui/FontProperties.h>
#include <NsGui/TextBox.h>
#include <NsGui/IRenderer.h>
#include <NsGui/XamlProvider.h>

#include <NsApp/ThemeProviders.h>

#include "ui/UIContext.h"
#include "ui/noesis/NoesisLocalXamlProvider.h"
#include "ui/noesis/NoesisLocalFontProvider.h"
#include "ui/noesis/NoesisTextureProvider.h"
#include "ui/noesis/NoesisGLRenderDevice.h"

#include "ui/noesis/NoesisWindowManager.h"
#include "ui/noesis/code_behind/SackContainerPanel.h"

#include "resources/ResourceManager.h"

#include "input/InputDispatcher.h"

#include "NoesisInputMappings.inl"
/*
#include <NsApp/EmbeddedXamlProvider.h>
#include <NsApp/EmbeddedFontProvider.h>
#include <NsApp/ApplicationLauncher.h>
#include <NsApp/EntryPoint.h>
#include <NsApp/Application.h>
#include <NsApp/Window.h>
#include <NsApp/RichText.h>*/


constexpr const char* XAML_ROOT = "data\\ui\\xaml";
constexpr const char* FONT_ROOT = "data\\ui\\fonts";

NoesisGuiContext* sNoesisGuiContext = nullptr;

static Noesis::Ptr<Noesis::RenderDevice> sRenderDevice = nullptr;


static UnorderedFlatMap<GameUIPanel, const char*> sNoesisGuiViewToFilename = {
    { GameUIPanel::Inventory, "inventory/inventory.xaml" },
    { GameUIPanel::SackContainer, "inventory/sack_container.xaml" }
};
static_assert(e_count(GameUIPanel) == 2, "Update the filenames");

NoesisGuiContext::NoesisGuiContext(UIContext& uiContext) : mUIContext(uiContext) {
    ASSERT_RENDER_THREAD();
    
    assert(!sNoesisGuiContext);
    sNoesisGuiContext = this;
  
    Noesis::SetLogHandler([](const char*, uint32_t, uint32_t level, const char*, const char* msg) {
        assert(level <= 4); // Noesis only has 5 levels (0-4)
        // Maps 1:1 with vorb::LoggingLevel
        vorb::LoggingLevel vLevel = (vorb::LoggingLevel)level;

        // [TRACE] [DEBUG] [INFO] [WARNING] [ERROR]
        const char* prefixes[] = { "T", "D", "I", "W", "E" };
        LOG_MSG(vLevel, "[NOESIS/{}] {}\n", prefixes[level], msg);
    });

    Noesis::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);

    // Noesis initialization. This must be the first step before using any NoesisGUI functionality
    Noesis::GUI::Init();

    Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisLocalXamlProvider>(ResourceManager::get().getIoManager(), XAML_ROOT));
    Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisLocalFontProvider>(ResourceManager::get().getIoManager(), FONT_ROOT));
    Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisTextureProvider>()); 
    
    // Set providers for the theme
    NoesisApp::SetThemeProviders();

    const char* fonts[] = {
        "titilium_bold",
        "Arial",
        "Segoe UI Emoji",           // Windows 10 Emojis
        "Arial Unicode MS",         // Almost everything (but part of MS Office, not Windows)
        "Microsoft Sans Serif",     // Unicode scripts excluding Asian scripts
        "Microsoft YaHei",          // Chinese
        "Gulim",                    // Korean
        "MS Gothic",                // Japanese};
    };
    Noesis::GUI::SetFontFallbacks(fonts, NS_COUNTOF(fonts));
    Noesis::GUI::SetFontDefaultProperties(15.0f, Noesis::FontWeight_Normal, Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);

    Noesis::GUI::LoadApplicationResources(NoesisApp::Theme::DarkBlue());

    sRenderDevice = Noesis::Ptr<Noesis::RenderDevice>(new NoesisGLRenderDevice());

    // Register code-behind classes
    Noesis::RegisterComponent<NoesisWindowManager>();
    NoesisWindowManager::RegisterChildren();
    Noesis::RegisterComponent<SackContainerViewModel>();
    Noesis::RegisterComponent<SackContainerPanel>();
    SackContainerPanel::RegisterChildren();

    initView();

    // TODO: REMOVE
    //toggleView(GameUIPanel::Inventory);

    //A view is needed to render the user interface and interact with it.A view holds a tree of elements.
    // The easiest way to build interface trees is by loading them from XAML files.This can be done using the helper function LoadXaml.
    // Once the XAML is loaded you must create a view with it and specify its dimensions.Remember to sync the size of the view each time your window or surface is resized.

    // Events
    /*Slider* slider = view->GetContent()->FindName<Slider>("Luminance");
    slider->ValueChanged() += &LuminanceChanged;*/
}

NoesisGuiContext::~NoesisGuiContext() {
    assert(sNoesisGuiContext == this);
    sNoesisGuiContext = nullptr;
    mView->GetRenderer()->Shutdown();
    Noesis::GUI::Shutdown();
}

bool NoesisGuiContext::processInput(SDL_Event* e) {
    ASSERT_RENDER_THREAD();

    switch (e->type) {
        case SDL_KEYDOWN: {

            // TODO: replace with a more general way to close UI
            if (e->key.keysym.sym == SDLK_q) {
                mWindowManager->RemoveWindow(GameUIPanel::SackContainer);
                return true;
            }
            auto&& it = sSdlKeycodeToNoesisKey.find(e->key.keysym.sym);
            if (it != sSdlKeycodeToNoesisKey.end()) {
                if (mView->KeyDown(it->second)) return true;
            }
            break;
        }
        case SDL_KEYUP: {
            auto&& it = sSdlKeycodeToNoesisKey.find(e->key.keysym.sym);
            if (it != sSdlKeycodeToNoesisKey.end()) {
                if (mView->KeyUp(it->second)) return true;
            }
            break;
        }
        case SDL_MOUSEMOTION:
            if (mView->MouseMove(e->motion.x, e->motion.y)) return true;
            break;
        case SDL_MOUSEBUTTONDOWN: {
            auto&& it = sVorbMouseButtonToNoesisMouseButton.find((vui::MouseButton)e->button.button);
            if (it != sVorbMouseButtonToNoesisMouseButton.end()) {
                if (e->button.clicks == 2) {
                    if (mView->MouseDoubleClick(e->button.x, e->button.y, it->second)) return true;
                }
                else {
                    if (mView->MouseButtonDown(e->button.x, e->button.y, it->second)) return true;
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONUP:{
            auto&& it = sVorbMouseButtonToNoesisMouseButton.find((vui::MouseButton)e->button.button);
            if (it != sVorbMouseButtonToNoesisMouseButton.end()) {
                if (mView->MouseButtonUp(e->button.x, e->button.y, it->second)) return true;
            }
            break;
        }
        case SDL_MOUSEWHEEL: {
            const i32v2 mousePos = vui::InputDispatcher::mouse.getPosition();
            if (mView->MouseWheel(mousePos.x, mousePos.y, e->wheel.y)) return true;
            break;
        }
        default:
            // Unrecognized event
            break;
    }
}

void NoesisGuiContext::updateAndRender() {
    ASSERT_RENDER_THREAD();

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    const double timeSeconds = std::chrono::duration<double>(duration).count();

    bool renderView = mWindowManager->update();

    if (mView->Update(timeSeconds)) {
        PreciseTimer timer;
        mView->GetRenderer()->UpdateRenderTree();
        LOG_CRITICAL("UpdateRenderTree {} ", timer.stop());
    }

    if (!renderView) {
        return;
    }

    mView->GetRenderer()->RenderOffscreen();
    ui32v2 dims = mUIContext.getWindowDims();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dims.x, dims.y);
    glDisable(GL_SCISSOR_TEST);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    mView->GetRenderer()->Render();

    checkGlError("NoesisGuiContext::updateAndRender");

    // Clear any mInUse program so we don't assume it is bound
    // TODO: Better state API
    vg::GLProgram::unuse();
}

void NoesisGuiContext::initView() {
    ASSERT_RENDER_THREAD();
    const char* viewNameStr = "main_view.xaml";
    LOG_DEBUG("INIT VIEW {}", viewNameStr);

    PreciseTimer timer;
    Noesis::Ptr<Noesis::FrameworkElement> xaml = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(viewNameStr);
    LOG_DEBUG(" LOAD {} ", timer.stop()); timer.start();
    mView = Noesis::GUI::CreateView(xaml);
    LOG_DEBUG(" VIEW {} ", timer.stop()); timer.start();
    mView->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
    const ui32v2 dims = mUIContext.getWindowDims();
    mView->SetSize(dims.x, dims.y);
    mView->GetRenderer()->Init(sRenderDevice);
    LOG_DEBUG(" INIT {} ", timer.stop()); timer.start();

    mWindowManager = mView->GetContent()->FindName<NoesisWindowManager>("WindowManager");
}

void NoesisGuiContext::toggleView(GameUIPanel viewName) {
    mWindowManager->ToggleWindow(viewName);
}

void NoesisGuiContext::disableView(GameUIPanel viewName) {
    mWindowManager->RemoveWindow(viewName);
}

void NoesisGuiContext::enableView(GameUIPanel viewName) {
    mWindowManager->AddWindow(viewName);
}

void NoesisGuiContext::updateItemSackUI(RenderThreadSharedComponentDataPtr data) {

    if (!data) {
        mWindowManager->RemoveWindow(GameUIPanel::SackContainer);
        return;
    }

    SackContainerViewModel* containerVM = mWindowManager->GetSackContainerViewModel();
    if (!containerVM) {
        // Add sack container
        mWindowManager->AddWindow(GameUIPanel::SackContainer);
        containerVM = mWindowManager->GetSackContainerViewModel();
    }

   
    if (mItemSackData && mItemSackData->uid != data->uid) [[unlikely]] {
        // Close old UI (RARE CASE)
        mWindowManager->RemoveWindow(GameUIPanel::SackContainer);
        mWindowManager->AddWindow(GameUIPanel::SackContainer);
        containerVM = mWindowManager->GetSackContainerViewModel();
    }

    mItemSackData = std::move(data);
    if (mItemSackData->wasDestroyed) {
        // Destroyed by main thread
        mItemSackData.reset();
        mWindowManager->RemoveWindow(GameUIPanel::SackContainer);
        return;
    }

    // Send copy of data to UI
    {
        std::lock_guard lock(mItemSackData->resourceMutex);
        containerVM->updateItems(mItemSackData->getItemSackData().itemStacks);
    }
}
