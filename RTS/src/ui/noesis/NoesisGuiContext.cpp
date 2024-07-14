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

#include "ui/UIContext.h"
#include "ui/noesis/NoesisLocalXamlProvider.h"
#include "ui/noesis/NoesisLocalFontProvider.h"
#include "ui/noesis/NoesisGLRenderDevice.h"
#include <NsApp/LocalTextureProvider.h>
//#include <NsApp/ThemeProviders.h>

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


constexpr const char* XAML_ROOT = "data/ui/xaml";
constexpr const char* FONT_ROOT = "data/ui/fonts";
constexpr const char* TEXTURES_ROOT = "data/ui/textures";
constexpr const char* RESOURCES_ROOT = "data/ui/resources";

NoesisGuiContext* sNoesisGuiContext = nullptr;

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

    Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisLocalXamlProvider>(XAML_ROOT));
    Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisLocalFontProvider>(FONT_ROOT));
    Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>(TEXTURES_ROOT)); 
    
    // Set providers for the theme
    //NoesisApp::SetThemeProviders();

    const char* fonts[] = { "data/ui/fonts/titilium_bold" };
    Noesis::GUI::SetFontFallbacks(fonts, 1);
    Noesis::GUI::SetFontDefaultProperties(15.0f, Noesis::FontWeight_Normal, Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);

    Noesis::GUI::LoadApplicationResources(RESOURCES_ROOT);

    //A view is needed to render the user interface and interact with it.A view holds a tree of elements.
    // The easiest way to build interface trees is by loading them from XAML files.This can be done using the helper function LoadXaml.
    // Once the XAML is loaded you must create a view with it and specify its dimensions.Remember to sync the size of the view each time your window or surface is resized.
    Noesis::Ptr<Noesis::FrameworkElement> xaml = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>("Reflections.xaml");
    mView = Noesis::GUI::CreateView(xaml);
    mView->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
    mView->SetSize(1024, 768);

    Noesis::Ptr<Noesis::RenderDevice> device(new NoesisGLRenderDevice());
    mView->GetRenderer()->Init(device);

    // Register code-behind classes
    /*Noesis::RegisterComponent<Scoreboard::MainWindow>();
    Noesis::RegisterComponent<Scoreboard::App>();
    Noesis::RegisterComponent<Scoreboard::ThousandConverter>();
    Noesis::RegisterComponent<EnumConverter<Scoreboard::Team>>();
    Noesis::RegisterComponent<EnumConverter<Scoreboard::Class>>();*/

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

void NoesisGuiContext::processInput(SDL_Event* e) {
    assert(mView);
    switch (e->type) {
        case SDL_KEYDOWN: {
            auto&& it = sSdlKeycodeToNoesisKey.find(e->key.keysym.sym);
            if (it != sSdlKeycodeToNoesisKey.end()) {
                mView->KeyDown(it->second);
            }
            break;
        }
        case SDL_KEYUP: {
            auto&& it = sSdlKeycodeToNoesisKey.find(e->key.keysym.sym);
            if (it != sSdlKeycodeToNoesisKey.end()) {
                mView->KeyUp(it->second);
            }
            break;
        }
        case SDL_MOUSEMOTION:
            mView->MouseMove(e->motion.x, e->motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN: {
            auto&& it = sVorbMouseButtonToNoesisMouseButton.find((vui::MouseButton)e->button.button);
            if (it != sVorbMouseButtonToNoesisMouseButton.end()) {
                if (e->button.clicks == 2) {
                    mView->MouseDoubleClick(e->button.x, e->button.y, it->second);
                }
                else {
                    mView->MouseButtonDown(e->button.x, e->button.y, it->second);
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONUP:{
            auto&& it = sVorbMouseButtonToNoesisMouseButton.find((vui::MouseButton)e->button.button);
            if (it != sVorbMouseButtonToNoesisMouseButton.end()) {
                mView->MouseButtonUp(e->button.x, e->button.y, it->second);
            }
            break;
        }
        case SDL_MOUSEWHEEL:
            // TODO: Is this right?
            mView->MouseWheel(e->wheel.x, e->wheel.y, e->wheel.direction == SDL_MOUSEWHEEL_NORMAL ? 1 : -1);
            break;
        default:
            // Unrecognized event
            break;
    }
}

void NoesisGuiContext::updateAndRender() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    const double timeSeconds = std::chrono::duration<double>(duration).count();

    if (mView->Update(timeSeconds)) {
        mView->GetRenderer()->UpdateRenderTree();
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
}
