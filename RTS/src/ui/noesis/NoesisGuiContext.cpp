#include "stdafx.h"
#include "NoesisGuiContext.h"

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

#include <NsApp/LocalTextureProvider.h>
#include <NsApp/ThemeProviders.h>

#include "ui/UIContext.h"
#include "ui/noesis/NoesisLocalXamlProvider.h"
#include "ui/noesis/NoesisLocalFontProvider.h"
#include "ui/noesis/NoesisGLRenderDevice.h"

#include "ui/noesis/code_behind/InventoryUI.h"

#include "resources/ResourceManager.h"

#include "input/MouseEventManager.h"

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
constexpr const char* TEXTURES_ROOT = "data\\ui\\xaml";

NoesisGuiContext* sNoesisGuiContext = nullptr;

static Noesis::Ptr<Noesis::RenderDevice> sRenderDevice = nullptr;


static UnorderedFlatMap<NoesisGuiView, const char*> sNoesisGuiViewToFilename = {
    { NoesisGuiView::Inventory, "inventory/inventory.xaml" }
};
static_assert(e_count(NoesisGuiView) == 1, "Update the filenames");

NoesisGuiContext::NoesisGuiContext(UIContext& uiContext) : mUIContext(uiContext) {
    
    assert(!sNoesisGuiContext);
    sNoesisGuiContext = this;
  
    Noesis::SetLogHandler([](const char*, uint32_t, uint32_t level, const char*, const char* msg)
    {
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
    Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>(TEXTURES_ROOT)); 
    
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
    Noesis::RegisterComponent<InventoryUI>();

    //A view is needed to render the user interface and interact with it.A view holds a tree of elements.
    // The easiest way to build interface trees is by loading them from XAML files.This can be done using the helper function LoadXaml.
    // Once the XAML is loaded you must create a view with it and specify its dimensions.Remember to sync the size of the view each time your window or surface is resized.

    addView(NoesisGuiView::Inventory);
    

    // Events
    /*Slider* slider = view->GetContent()->FindName<Slider>("Luminance");
    slider->ValueChanged() += &LuminanceChanged;*/
}

NoesisGuiContext::~NoesisGuiContext() {
    assert(sNoesisGuiContext == this);
    sNoesisGuiContext = nullptr;
    for (auto& [name, view] : mViews) {
        view->GetRenderer()->Shutdown();
    }
    Noesis::GUI::Shutdown();
}

void NoesisGuiContext::processInput(SDL_Event* e) {
    switch (e->type) {
        case SDL_KEYDOWN: {
            auto&& it = sSdlKeycodeToNoesisKey.find(e->key.keysym.sym);
            if (it != sSdlKeycodeToNoesisKey.end()) {
                for (auto& [name, view] : mViews) {
                    view->KeyDown(it->second);
                }
            }
            break;
        }
        case SDL_KEYUP: {
            auto&& it = sSdlKeycodeToNoesisKey.find(e->key.keysym.sym);
            for (auto& [name, view] : mViews) {
                view->KeyUp(it->second);
            }
            break;
        }
        case SDL_MOUSEMOTION:
            for (auto& [name, view] : mViews) {
                view->MouseMove(e->motion.x, e->motion.y);
            }
            break;
        case SDL_MOUSEBUTTONDOWN: {
            auto&& it = sVorbMouseButtonToNoesisMouseButton.find((vui::MouseButton)e->button.button);
            if (it != sVorbMouseButtonToNoesisMouseButton.end()) {
                if (e->button.clicks == 2) {
                    for (auto& [name, view] : mViews) {
                        view->MouseDoubleClick(e->button.x, e->button.y, it->second);
                    }
                }
                else {
                    for (auto& [name, view] : mViews) {
                        view->MouseButtonDown(e->button.x, e->button.y, it->second);
                    }
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONUP:{
            auto&& it = sVorbMouseButtonToNoesisMouseButton.find((vui::MouseButton)e->button.button);
            if (it != sVorbMouseButtonToNoesisMouseButton.end()) {
                for (auto& [name, view] : mViews) {
                    view->MouseButtonUp(e->button.x, e->button.y, it->second);
                }
            }
            break;
        }
        case SDL_MOUSEWHEEL:
            for (auto& [name, view] : mViews) {
                // TODO: Is this right?
                view->MouseWheel(e->wheel.x, e->wheel.y, e->wheel.direction == SDL_MOUSEWHEEL_NORMAL ? 1 : -1);
            }
            break;
        default:
            // Unrecognized event
            break;
    }
}

void NoesisGuiContext::updateAndRender() {
    if (!mViews.size()) {
        return;
    }

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    const double timeSeconds = std::chrono::duration<double>(duration).count();

    for (auto& [name, view] : mViews) {
        if (view->Update(timeSeconds)) {
            view->GetRenderer()->UpdateRenderTree();
        }
        view->GetRenderer()->RenderOffscreen();
    }

    ui32v2 dims = mUIContext.getWindowDims();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dims.x, dims.y);
    glDisable(GL_SCISSOR_TEST);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    for (auto& [name, view] : mViews) {
        view->GetRenderer()->Render();
    }

    checkGlError("NoesisGuiContext::updateAndRender");

    // Clear any mInUse program so we don't assume it is bound
    // TODO: Better state API
    vg::GLProgram::unuse();
}

void NoesisGuiContext::addView(NoesisGuiView viewName) {
    const char* viewNameStr = sNoesisGuiViewToFilename.at(viewName);
    Noesis::Ptr<Noesis::FrameworkElement> xaml = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(viewNameStr);
    Noesis::Ptr<Noesis::IView> newView = Noesis::GUI::CreateView(xaml);
    newView->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
    const ui32v2 dims = mUIContext.getWindowDims();
    newView->SetSize(dims.x, dims.y);
    newView->GetRenderer()->Init(sRenderDevice);
    assert(!mViews.contains(viewName));
    mViews[viewName] = newView;
}

void NoesisGuiContext::removeView(NoesisGuiView viewName) {
    auto&& it = mViews.find(viewName);
    if (it != mViews.end()) {
        it->second->GetRenderer()->Shutdown();
        mViews.erase(it);
    }
    else {
        LOG_WARN("Tried to remove inactive view {}", (int)viewName);
    }
}

void NoesisGuiContext::toggleView(NoesisGuiView viewName) {
    if (mViews.contains(viewName)) {
        removeView(viewName);
    }
    else {
        addView(viewName);
    }
}
