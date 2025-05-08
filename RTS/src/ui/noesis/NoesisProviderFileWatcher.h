#pragma once


#include <NsCore/Noesis.h>
#include <NsCore/String.h>
#include <NsCore/HashMap.h>
#include "filesystem/FileSystemWatcher.h"

class NoesisProviderFileWatcher : public Noesis::BaseRefCounted
{
public:
    void Watch(const char* path, const Noesis::Uri& uri)
    {
#if NS_XAML_RELOAD_ENABLED == 1
        if (mWatchedFiles.Insert(path, uri).second)
        {
            Noesis::FixedString<512> directory;

            int pos = Noesis::StrFindLast(path, "/");
            if (pos != -1)
            {
                directory.Assign(path, pos);
            }

            if (mWatchedDirectories.Insert(directory.Str()).second) {

                mFileSystemWatchers.Insert(new filewatch::FileWatch<std::string>(
                    std::string(path),
                    [this, dir = std::string(directory.Str())](const std::string& path, const filewatch::Event change_type) {
                    ChangeNotification(dir + "/" + path);
                }), Noesis::String(directory.Str()));
            }
        }
#else
        UNUSED(path, uri);
#endif
    }

    ~NoesisProviderFileWatcher()
    {
        for (auto& [watcher, directory] : mFileSystemWatchers) {
            delete watcher;
        }
    }

    Noesis::Delegate<void(const Noesis::Uri&)>& Changed()
    {
        return mChangedCallback;
    }

private:
    void ChangeNotification(const std::string& path) {
        auto it = mWatchedFiles.Find(Noesis::String(path.c_str()));
        if (it != mWatchedFiles.End())
        {
            mChangedCallback(it->value);
        }
    }
    Noesis::HashMap<filewatch::FileWatch<std::string>*, Noesis::String/*directory*/> mFileSystemWatchers;
    Noesis::HashSet<Noesis::String> mWatchedDirectories;
    Noesis::HashMap<Noesis::String, Noesis::Uri> mWatchedFiles;
    Noesis::Delegate<void(const Noesis::Uri&)> mChangedCallback;
};