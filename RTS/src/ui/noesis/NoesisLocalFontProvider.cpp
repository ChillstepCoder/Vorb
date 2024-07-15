#include "stdafx.h"
#include "NoesisLocalFontProvider.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsGui/Stream.h>
#include <NsGui/Uri.h>
#include <NsCore/UTF8.h>

#include <Vorb/io/IOManager.h>

using namespace Noesis;

struct FindData
{
    char filename[512];
    char extension[16];
    void* handle;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
bool FindFirst(const char* directory, const char* extension, FindData& findData)
{
    char fullPath[sizeof(findData.filename)];
    StrCopy(fullPath, sizeof(fullPath), directory);
    StrAppend(fullPath, sizeof(fullPath), "/*");
    StrAppend(fullPath, sizeof(fullPath), extension);

    uint16_t u16str[sizeof(fullPath)];
    uint32_t numChars = UTF8::UTF8To16(fullPath, u16str, sizeof(fullPath));
    NS_ASSERT(numChars <= sizeof(fullPath));

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((LPCWSTR)u16str, FindExInfoBasic, &fd, FindExSearchNameMatch, 0, 0);
    if (h != INVALID_HANDLE_VALUE)
    {
        numChars = UTF8::UTF16To8((uint16_t*)fd.cFileName, findData.filename, sizeof(fullPath));
        NS_ASSERT(numChars <= sizeof(fullPath));
        StrCopy(findData.extension, sizeof(findData.extension), extension);
        findData.handle = h;
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool FindNext(FindData& findData) {
    WIN32_FIND_DATAW fd;
    int res = FindNextFileW(findData.handle, &fd);

    if (res)
    {
        const int MaxFilename = sizeof(findData.filename);
        uint32_t n = UTF8::UTF16To8((uint16_t*)fd.cFileName, findData.filename, MaxFilename);
        NS_ASSERT(n <= MaxFilename);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void FindClose(FindData& findData) {
    int r = ::FindClose(findData.handle);
    NS_ASSERT(r != 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisLocalFontProvider::NoesisLocalFontProvider(vio::IOManager& ioManager, const char* rootPath) : mIOManager(ioManager) {
    if (!ioManager.resolvePath(vio::Path(rootPath), mRootPath)) {
        panic("Failed to resolve NoesisLocalFontProvider root path: {}", rootPath);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisLocalFontProvider::ScanFolder(const Uri& folder)
{
    char uri[512] = "";

    if (mRootPath.isValid())
    {
        Noesis::StrCopy(uri, sizeof(uri), mRootPath.getCString());
        Noesis::StrAppend(uri, sizeof(uri), "/");
    }

    FixedString<512> path;
    folder.GetPath(path);

    Noesis::StrAppend(uri, sizeof(uri), path.Str());

    ScanFolder(uri, folder, ".ttf");
    ScanFolder(uri, folder, ".otf");
    ScanFolder(uri, folder, ".ttc");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Stream> NoesisLocalFontProvider::OpenFont(const Uri& folder, const char* filename) const
{
    char uri[512] = "";

    if (mRootPath.isValid())
    {
        Noesis::StrCopy(uri, sizeof(uri), mRootPath.getCString());
        Noesis::StrAppend(uri, sizeof(uri), "/");
    }

    FixedString<512> path;
    folder.GetPath(path);

    StrAppend(uri, sizeof(uri), path.Str());
    StrAppend(uri, sizeof(uri), "/");
    StrAppend(uri, sizeof(uri), filename);

    return OpenFileStream(uri);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisLocalFontProvider::ScanFolder(const char* path, const Uri& folder, const char* ext)
{

    FindData findData;

    if (FindFirst(path, ext, findData))
    {
        do
        {
            RegisterFont(folder, findData.filename);
        } while (FindNext(findData));

        FindClose(findData);
    }
}
