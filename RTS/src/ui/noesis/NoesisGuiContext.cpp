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

#include <NsApp/LocalTextureProvider.h>
#include <NsApp/ThemeProviders.h>

#include "ui/UIContext.h"
#include "ui/noesis/NoesisLocalXamlProvider.h"
#include "ui/noesis/NoesisLocalFontProvider.h"
#include "ui/noesis/NoesisGLRenderDevice.h"

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
constexpr const char* TEXTURES_ROOT = "data\\ui\\xaml";

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
    Noesis::RegisterComponent<SackContainerPanel>();
    SackContainerPanel::RegisterChildren();


    for (int i = 0; i < e_count(GameUIPanel); ++i) {
        initView((GameUIPanel)i);
    }

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
    mActiveViews.clear();
    for (auto& view : mViews) {
        view->GetRenderer()->Shutdown();
    }
    Noesis::GUI::Shutdown();
}

bool NoesisGuiContext::processInput(SDL_Event* e) {
    ASSERT_RENDER_THREAD();

    switch (e->type) {
        case SDL_KEYDOWN: {

            // TODO: replace with a more general way to close UI
            if (e->key.keysym.sym == SDLK_q) {
                mViewWantsActive[e_cast(GameUIPanel::SackContainer)].store(false);
                return true;
            }
            auto&& it = sSdlKeycodeToNoesisKey.find(e->key.keysym.sym);
            if (it != sSdlKeycodeToNoesisKey.end()) {
                if (forEachActiveViewHandleInput([&](Noesis::IView& view) {
                    return view.KeyDown(it->second);
                })) return true;
            }
            break;
        }
        case SDL_KEYUP: {
            auto&& it = sSdlKeycodeToNoesisKey.find(e->key.keysym.sym);
            if (forEachActiveViewHandleInput([&](Noesis::IView& view) {
                return view.KeyUp(it->second);
            })) return true;
            break;
        }
        case SDL_MOUSEMOTION:
            if (forEachActiveViewHandleInput([&](Noesis::IView& view) {
                LOG_DEBUG("{} {}", e->motion.x, e->motion.y);
                return view.MouseMove(e->motion.x, e->motion.y);
            })) return true;
            break;
        case SDL_MOUSEBUTTONDOWN: {
            auto&& it = sVorbMouseButtonToNoesisMouseButton.find((vui::MouseButton)e->button.button);
            if (it != sVorbMouseButtonToNoesisMouseButton.end()) {
                if (e->button.clicks == 2) {
                    if (forEachActiveViewHandleInput([&](Noesis::IView& view) {
                        return view.MouseDoubleClick(e->button.x, e->button.y, it->second);
                    })) return true;
                }
                else {
                    if (forEachActiveViewHandleInput([&](Noesis::IView& view) {
                        return view.MouseButtonDown(e->button.x, e->button.y, it->second);
                    })) return true;
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONUP:{
            auto&& it = sVorbMouseButtonToNoesisMouseButton.find((vui::MouseButton)e->button.button);
            if (it != sVorbMouseButtonToNoesisMouseButton.end()) {
                if (forEachActiveViewHandleInput([&](Noesis::IView& view) {
                    return view.MouseButtonUp(e->button.x, e->button.y, it->second);
                })) return true;
            }
            break;
        }
        case SDL_MOUSEWHEEL: {
            const i32v2 mousePos = vui::InputDispatcher::mouse.getPosition();
            if (forEachActiveViewHandleInput([&](Noesis::IView& view) {
                LOG_DEBUG("WEEL {} {}", mousePos.x, mousePos.y);
                return view.MouseWheel(mousePos.x, mousePos.y, e->wheel.y);
            })) return true;
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

    // Prevent having to check atomic twice, so we dont get mismatched render
    Noesis::IView* renderViews[e_count(GameUIPanel)];
    i32 renderViewCount = 0;

    for (i32 i = 0; i < e_count(GameUIPanel); ++i) {
        // Only check the atomic once per frame
        if (mViewWantsActive[i]) {
            Noesis::IView* view = mViews[i];
            if (!mViewWasActive[i]) {
                // Grab input focus
                view->Activate();
                // Force set mouse position on entry
                const i32v2 mousePos = vui::InputDispatcher::mouse.getPosition();
               // view->MouseMove(mousePos.x, mousePos.y);
                LOG_CRITICAL("ACTIVATE {} {}", mousePos.x, mousePos.y);
                mViewWasActive[i] = true;
            }
            // Always update render tree when we first become active
            if (view->Update(timeSeconds) || !mViewWasActive[i]) {
                PreciseTimer timer;
                view->GetRenderer()->UpdateRenderTree();
                LOG_CRITICAL("UpdateRenderTree {} ", timer.stop());
            }
            view->GetRenderer()->RenderOffscreen();
            renderViews[renderViewCount++] = view;
        }
        else if (mViewWasActive[i]) {
            // Release input focus
            mViews[i]->Deactivate();
            mViewWasActive[i] = false;
        }
    }

    if (!renderViewCount) {
        return;
    }

    ui32v2 dims = mUIContext.getWindowDims();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dims.x, dims.y);
    glDisable(GL_SCISSOR_TEST);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    for (i32 i = 0; i < renderViewCount; ++i) {
        renderViews[i]->GetRenderer()->Render();
    }

    checkGlError("NoesisGuiContext::updateAndRender");

    // Clear any mInUse program so we don't assume it is bound
    // TODO: Better state API
    vg::GLProgram::unuse();
}

void NoesisGuiContext::initView(GameUIPanel viewName) {
    ASSERT_RENDER_THREAD();
    const char* viewNameStr = sNoesisGuiViewToFilename.at(viewName);
    LOG_DEBUG("INIT VIEW {}", viewNameStr);

    PreciseTimer timer;
    Noesis::Ptr<Noesis::FrameworkElement> xaml = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(viewNameStr);
    LOG_DEBUG(" LOAD {} ", timer.stop()); timer.start();
    Noesis::Ptr<Noesis::IView> newView = Noesis::GUI::CreateView(xaml);
    LOG_DEBUG(" VIEW {} ", timer.stop()); timer.start();
    newView->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
    const ui32v2 dims = mUIContext.getWindowDims();
    newView->SetSize(dims.x, dims.y);
    newView->GetRenderer()->Init(sRenderDevice);
    LOG_DEBUG(" INIT {} ", timer.stop()); timer.start();
    mViews[e_cast(viewName)] = newView;
}

bool NoesisGuiContext::forEachActiveViewHandleInput(std::function<bool(Noesis::IView&)> func) {
    std::lock_guard lock(mActiveViewsMutex);
    for (auto& view : mActiveViews) {
        if (func(*view)) {
            return true;
        }
    }
    return false;
}

void NoesisGuiContext::onPanelActiveChanged(GameUIPanel panel, bool active) {
    if (active) {
        std::lock_guard lock(mActiveViewsMutex);
        // Make sure its not already active
        for (size_t i = 0; i < mActiveViews.size(); ++i) {
            if (mActiveViews[i] == mViews[e_cast(panel)]) {
                return;
            }
        }
        mActiveViews.emplace_back(mViews[e_cast(panel)]);
    }
    else {
        std::lock_guard lock(mActiveViewsMutex);
        for (size_t i = 0; i < mActiveViews.size(); ++i) {
            if (mActiveViews[i] == mViews[e_cast(panel)]) {
                mActiveViews[i] = mActiveViews.back();
                mActiveViews.pop_back();
                break;
            }
        }
    }
}

void NoesisGuiContext::toggleView(GameUIPanel viewName) {
    const int newActive = mViewWantsActive[e_cast(viewName)].fetch_xor(1, std::memory_order_relaxed) ^ 1;
    onPanelActiveChanged(viewName, (bool)newActive);
}

void NoesisGuiContext::updateItemSackUI(RenderThreadSharedComponentDataPtr data) {

    Noesis::IView& view = *mViews[e_cast(GameUIPanel::SackContainer)];
    // Get the root element of the view
    Noesis::FrameworkElement* root = view.GetContent();

    // Find your InventoryUI control
    SackContainerPanel* panel = root->FindName<SackContainerPanel>("RootPanel");
    if (!panel) {
        panic("RootPanel missing from sack_container.xaml");
    }

    if (!data) {
        mItemSackData.reset();
    }
    else {
        if (mItemSackData && mItemSackData->uid != data->uid) [[unlikely]] {
            // Close old UI
            panel->reset();
        }

        mItemSackData = std::move(data);
        if (mItemSackData->wasDestroyed) {
            mItemSackData.reset();
        }
    }

    // Set UI active
    ExclusiveCacheLine<std::atomic_int>& wantsActive = mViewWantsActive[e_cast(GameUIPanel::SackContainer)];
    if (mItemSackData) {
        wantsActive.store(1, std::memory_order_relaxed);
        onPanelActiveChanged(GameUIPanel::SackContainer, true);

        // Send copy of data to UI
        {
            std::lock_guard lock(mItemSackData->resourceMutex);
            panel->updateItems(mItemSackData->getItemSackData().itemStacks);
        }
    }
    else {
        wantsActive.store(0, std::memory_order_relaxed);
        onPanelActiveChanged(GameUIPanel::SackContainer, false);
        panel->reset();
    }
}
