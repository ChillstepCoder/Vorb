#pragma once

#include <Windows.h>
#include <string>
#include <shobjidl.h> 

// TODO: Common util
template <typename T>
constexpr auto sizeof_array(const T& iarray) {
    return (sizeof(iarray) / sizeof(iarray[0]));
}

enum class FileBrowserTypes {
    JPG,
    FBX,
    ALL
};

constexpr COMDLG_FILTERSPEC specs[] = {
    { L"jpeg", L"*.jpg;*.jpeg" }, // JPG
    { L"fbx", L"*.fbx" }, // FBX
    { L"ALL", L"*.*" }, // ALL
};

// TODO: Replace with https://github.com/mlabbe/nativefiledialog or similar

namespace NativeFileBrowser {

    std::string openFile(FileBrowserTypes allowedTypes) {
        static std::string sSelectedFile;
        //  CREATE FILE OBJECT INSTANCE
        HRESULT f_SysHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(f_SysHr))
            return "";

        // CREATE FileOpenDialog OBJECT
        IFileOpenDialog* f_FileSystem;
        f_SysHr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&f_FileSystem));
        if (FAILED(f_SysHr)) {
            CoUninitialize();
            return "";
        }

        if (allowedTypes == FileBrowserTypes::ALL) {
            f_FileSystem->SetFileTypes(sizeof_array(specs), specs);
        }
        else {
            f_FileSystem->SetFileTypes(1, &specs[e_cast(allowedTypes)]);
        }

        //  SHOW OPEN FILE DIALOG WINDOW
        f_SysHr = f_FileSystem->Show(NULL);
        if (FAILED(f_SysHr)) {
            f_FileSystem->Release();
            CoUninitialize();
            return "";
        }

        //  RETRIEVE FILE NAME FROM THE SELECTED ITEM
        IShellItem* f_Files;
        f_SysHr = f_FileSystem->GetResult(&f_Files);
        if (FAILED(f_SysHr)) {
            f_FileSystem->Release();
            CoUninitialize();
            return "";
        }

        //  STORE AND CONVERT THE FILE NAME
        PWSTR f_Path;
        f_SysHr = f_Files->GetDisplayName(SIGDN_FILESYSPATH, &f_Path);
        if (FAILED(f_SysHr)) {
            f_Files->Release();
            f_FileSystem->Release();
            CoUninitialize();
            return "";
        }

        //  FORMAT AND STORE THE FILE PATH
        std::wstring path(f_Path);
        std::string filePath(path.begin(), path.end());
       
        ////  FORMAT STRING FOR EXECUTABLE NAME
        //const size_t slash = sFilePath.find_last_of("/\\");
        //sSelectedFile = sFilePath.substr(slash + 1);

        //  SUCCESS, CLEAN UP
        CoTaskMemFree(f_Path);
        f_Files->Release();
        f_FileSystem->Release();
        CoUninitialize();

        return filePath;
    }
}