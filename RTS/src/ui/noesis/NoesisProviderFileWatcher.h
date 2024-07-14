#pragma once


#include <NsCore/Noesis.h>
#include "filesystem/FileSystemWatcher.h"

class NoesisProviderFileWatcher : public Noesis::BaseRefCounted
{
public:
    using FSW = FileSystemWatcher;

    void Watch(const char* path, const Noesis::Uri& uri)
    {
        if (mWatchedFiles.Insert(path, uri).second)
        {
            Noesis::FixedString<512> directory;

            int pos = Noesis::StrFindLast(path, "/");
            if (pos != -1)
            {
                directory.Assign(path, pos);
            }

            if (mWatchedDirectories.Insert(directory.Str()).second)
            {
                void* watcher = FSW::Create(directory.Str(), false);
                FSW::Changed(watcher) += MakeDelegate(this, &NoesisProviderFileWatcher::ChangeNotification);
                FSW::Created(watcher) += MakeDelegate(this, &NoesisProviderFileWatcher::ChangeNotification);
                mFileSystemWatchers.PushBack(watcher);
            }
        }
    }

    ~NoesisProviderFileWatcher()
    {
        for (void* watcher : mFileSystemWatchers)
        {
            FSW::Destroy(watcher);
        }
    }

    Noesis::Delegate<void(const Noesis::Uri&)>& Changed()
    {
        return mChangedCallback;
    }

private:
    void ChangeNotification(void* source, const char* filename)
    {
        Noesis::FixedString<512> path = FSW::Path(source);
        if (!path.Empty()) { path += "/"; }
        path += filename;

        auto it = mWatchedFiles.Find(path);
        if (it != mWatchedFiles.End())
        {
            mChangedCallback(it->value);
        }
    }

    Noesis::Vector<void*> mFileSystemWatchers;
    Noesis::HashSet<Noesis::String> mWatchedDirectories;
    Noesis::HashMap<Noesis::String, Noesis::Uri> mWatchedFiles;

    Noesis::Delegate<void(const Noesis::Uri&)> mChangedCallback;
};