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
#include <NsGui/XamlProvider.h>
#include <NsCore/StringUtils.h>

/*
#include <NsApp/EmbeddedXamlProvider.h>
#include <NsApp/EmbeddedFontProvider.h>
#include <NsApp/ApplicationLauncher.h>
#include <NsApp/EntryPoint.h>
#include <NsApp/Application.h>
#include <NsApp/Window.h>
#include <NsApp/RichText.h>*/

//
//class ProviderFileWatcher : public Noesis::BaseRefCounted
//{
//public:
//    using FSW = NoesisApp::FileSystemWatcher;
//
//    void Watch(const char* path, const Noesis::Uri& uri)
//    {
//        if (mWatchedFiles.Insert(path, uri).second)
//        {
//            Noesis::FixedString<512> directory;
//
//            int pos = Noesis::StrFindLast(path, "/");
//            if (pos != -1)
//            {
//                directory.Assign(path, pos);
//            }
//
//            if (mWatchedDirectories.Insert(directory.Str()).second)
//            {
//                void* watcher = FSW::Create(directory.Str(), false);
//                FSW::Changed(watcher) += MakeDelegate(this, &ProviderFileWatcher::ChangeNotification);
//                FSW::Created(watcher) += MakeDelegate(this, &ProviderFileWatcher::ChangeNotification);
//                mFileSystemWatchers.PushBack(watcher);
//            }
//        }
//    }
//
//    ~ProviderFileWatcher()
//    {
//        for (void* watcher : mFileSystemWatchers)
//        {
//            FSW::Destroy(watcher);
//        }
//    }
//
//    Noesis::Delegate<void(const Noesis::Uri&)>& Changed()
//    {
//        return mChangedCallback;
//    }
//
//private:
//    void ChangeNotification(void* source, const char* filename)
//    {
//        Noesis::FixedString<512> path = FSW::Path(source);
//        if (!path.Empty()) { path += "/"; }
//        path += filename;
//
//        auto it = mWatchedFiles.Find(path);
//        if (it != mWatchedFiles.End())
//        {
//            mChangedCallback(it->value);
//        }
//    }
//
//    Noesis::Vector<void*> mFileSystemWatchers;
//    Noesis::HashSet<Noesis::String> mWatchedDirectories;
//    Noesis::HashMap<Noesis::String, Noesis::Uri> mWatchedFiles;
//
//    Noesis::Delegate<void(const Noesis::Uri&)> mChangedCallback;
//};
//
//////////////////////////////////////////////////////////////////////////////////////////////////////
///// A XAML provider that searches in local directories
//////////////////////////////////////////////////////////////////////////////////////////////////////
//class LocalXamlProvider : public Noesis::XamlProvider
//{
//public:
//    LocalXamlProvider(const char* rootPath = "") {
//        Noesis::StrCopy(mRootPath, sizeof(mRootPath), rootPath);
//
//#ifdef NS_PROFILE
//        mWatcher = MakePtr<ProviderFileWatcher>();
//        mWatcher->Changed() += [this](const Uri& uri)
//        {
//            RaiseXamlChanged(uri);
//        };
//#endif
//    }
//    ~LocalXamlProvider() {
//
//    }
//
//private:
//    Noesis::Ptr<Noesis::Stream> LoadXaml(const Noesis::Uri& uri) override {
//
//    }
//
//private:
//    Noesis::Ptr<ProviderFileWatcher> mWatcher;
//    char mRootPath[512];
//};


NoesisGuiContext::NoesisGuiContext(UIContext& uiContext) : mUIContext(uiContext) {
    // TODO
    return;

   // Noesis::SetLogHandler([](const char*, uint32_t, uint32_t level, const char*, const char* msg)
   // {
   //     assert(level <= 4); // Noesis only has 5 levels (0-4)
   //     // Maps 1:1 with vorb::LoggingLevel
   //     vorb::LoggingLevel vLevel = (vorb::LoggingLevel)level;

   //     // [TRACE] [DEBUG] [INFO] [WARNING] [ERROR]
   //     const char* prefixes[] = { "T", "D", "I", "W", "E" };
   //     LOG_MSG(vLevel, "[NOESIS/{}] {}\n", prefixes[level], msg);
   // });

   // // Sets the active license
   // // TODO: Replace
   // Noesis::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);

   // // Noesis initialization. This must be the first step before using any NoesisGUI functionality
   // Noesis::GUI::Init();

   ///* Noesis::GUI::SetXamlProvider(MakePtr<LocalXamlProvider>("."));
   // Noesis::GUI::SetFontProvider(MakePtr<LocalFontProvider>("."));
   // Noesis::GUI::SetTextureProvider(MakePtr<LocalTextureProvider>("."));*/

   // const char* fonts[] = { "Fonts/#PT Root UI", "Arial", "Segoe UI Emoji" };
   // Noesis::GUI::SetFontFallbacks(fonts, 3);
   // Noesis::GUI::SetFontDefaultProperties(15.0f, Noesis::FontWeight_Normal, Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);
    
    // Every application must provide a ResourceDictionary containing styles and templates for all controls that appear in
    // the application. You can find more information in this guide.If you installed the theme providers available in the
    // Application Framework then this step is as simple as :
    //Noesis::GUI::LoadApplicationResources(NoesisApp::Theme::DarkBlue());

    //A view is needed to render the user interface and interact with it.A view holds a tree of elements.
    // The easiest way to build interface trees is by loading them from XAML files.This can be done using the helper function LoadXaml.
    // Once the XAML is loaded ymust create a view with it and specify its dimensions.Remember to sync the size of the view each time your window or surface is resized.
   /* Noesis::Ptr<Noesis::FrameworkElement> xaml = Noesis::GUI::LoadXaml<Noesis::FrameworkElement>("Reflections.xaml");
    Noesis::Ptr<Noesis::IView> view = Noesis::GUI::CreateView(xaml);
    view->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
    view->SetSize(1024, 768);

    Ptr<RenderDevice> device = NoesisApp::GLFactory::CreateDevice();
    view->GetRenderer()->Init(device);*/

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
