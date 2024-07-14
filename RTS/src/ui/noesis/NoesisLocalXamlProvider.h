#pragma once

#include <NSGui/XamlProvider.h>
#include <NSGui/Stream.h>
#include <NSGui/Uri.h>
#include <NsCore/String.h>
#include <NsCore/StringUtils.h>

#include "ui/noesis/NoesisProviderFileWatcher.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A XAML provider that searches in local directories
////////////////////////////////////////////////////////////////////////////////////////////////////
class NoesisLocalXamlProvider : public Noesis::XamlProvider
{
public:
    NoesisLocalXamlProvider(const char* rootPath = "") {
        Noesis::StrCopy(mRootPath, sizeof(mRootPath), rootPath);

#ifdef NS_PROFILE
        mWatcher = MakePtr<NoesisProviderFileWatcher>();
        mWatcher->Changed() += [this](const Uri& uri)
        {
            RaiseXamlChanged(uri);
        };
#endif
    }
    ~NoesisLocalXamlProvider() {
      
    }

private:
    Noesis::Ptr<Noesis::Stream> LoadXaml(const Noesis::Uri& uri) override {
        Noesis::FixedString<512> path;
        uri.GetPath(path);

        char filename[512];

        if (Noesis::StrIsEmpty(mRootPath))
        {
            Noesis::StrCopy(filename, sizeof(filename), path.Str());
        }
        else
        {
            Noesis::StrCopy(filename, sizeof(filename), mRootPath);
            Noesis::StrAppend(filename, sizeof(filename), "/");
            Noesis::StrAppend(filename, sizeof(filename), path.Str());
        }

        Noesis::Ptr<Noesis::Stream> stream = Noesis::OpenFileStream(filename);

#ifdef NS_PROFILE
        if (stream)
        {
            mWatcher->Watch(filename, uri);
        }
#endif

        return stream;
    }

private:
    Noesis::Ptr<NoesisProviderFileWatcher> mWatcher;
    char mRootPath[512];
};
