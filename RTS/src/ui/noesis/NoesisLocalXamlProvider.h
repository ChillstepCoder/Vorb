#pragma once

#include <NSGui/XamlProvider.h>
#include <NSGui/Stream.h>
#include <NSGui/Uri.h>
#include <NsCore/String.h>
#include <NsCore/StringUtils.h>

#include "ui/noesis/NoesisProviderFileWatcher.h"

#include <Vorb/io/IOManager.h>

#include "rendering/RenderThreadTasks.h"

#include <regex>

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A XAML provider that searches in local directories
////////////////////////////////////////////////////////////////////////////////////////////////////
class NoesisLocalXamlProvider : public Noesis::XamlProvider
{
public:
    NoesisLocalXamlProvider(vio::IOManager& ioManager, const char* rootPath = "") : mIOManager(ioManager) {
        if (!ioManager.resolvePath(vio::Path(rootPath), mRootPath)) {
            panic("Failed to resolve NoesisLocalXamlProvider root path: {}", rootPath);
        }
        mWatcher = Noesis::MakePtr<NoesisProviderFileWatcher>();
        mWatcher->Changed() += [this](const Noesis::Uri& uri)
        {
            struct ProviderTaskData {
                Noesis::Uri uri;
                NoesisLocalXamlProvider* provider;
            };
            ProviderTaskData* data = new ProviderTaskData({ uri, this });
            RenderThreadTasks::getInstance().addGenericTask([](class RenderContext&, void* data) {
                ProviderTaskData* pData = (ProviderTaskData*)data;
                pData->provider->RaiseXamlChanged(pData->uri);
            }, data);
        };
    }
    ~NoesisLocalXamlProvider() {
      
    }

private:
    Noesis::Ptr<Noesis::Stream> LoadXaml(const Noesis::Uri& uri) override {
        Noesis::FixedString<512> path;
        uri.GetPath(path);

        vio::Path filename = mRootPath / vio::Path(path.Str());


        Noesis::Ptr<Noesis::Stream> stream = Noesis::OpenFileStream(filename.getCString());

        if (stream)
        {
            nString replaced = std::regex_replace(filename.getString(), std::regex("\\\\"), "/");
            mWatcher->Watch(replaced.c_str(), uri);
        }

        return stream;
    }

private:
    Noesis::Ptr<NoesisProviderFileWatcher> mWatcher;
    vio::Path mRootPath;
    vio::IOManager& mIOManager;
};
