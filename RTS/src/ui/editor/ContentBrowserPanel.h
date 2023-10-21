#pragma once

#include <Vorb/Event.hpp>

#include "filesystem/FileSystem.h"
#include "resources/IAssetRepository.h"
#include "resources/ResourceManager.h"

#include "ui/editor/ContentBrowser/ContentBrowserItem.h"

#include <Vorb/ui/InputDispatcher.h>

enum class CONTENT_BROWSER_EVENT_TYPE {
    AssetCreated,
    AssetDeleted,
};
struct ContentBrowserEvent {
    AssetDescriptor assetDesc;
    std::filesystem::path path;
};
EVENT_DISPATCHER_TYPE(ContentBrowser, CONTENT_BROWSER_EVENT_TYPE, ContentBrowserEvent&);

class SelectionStack
{
public:
    void copyFrom(const SelectionStack& other)
    {
        mSelections.assign(other.begin(), other.end());
    }

    void copyFrom(const std::vector<UniqueId64>& other)
    {
        mSelections.assign(other.begin(), other.end());
    }

    void select(UniqueId64 handle)
    {
        if (isSelected(handle))
            return;

        mSelections.push_back(handle);
    }

    void deselect(UniqueId64 handle)
    {
        if (!isSelected(handle))
            return;

        for (auto it = mSelections.begin(); it != mSelections.end(); it++)
        {
            if (handle == *it)
            {
                mSelections.erase(it);
                break;
            }
        }
    }

    bool isSelected(UniqueId64 handle) const
    {
        for (const auto& selectedHandle : mSelections)
        {
            if (selectedHandle == handle)
                return true;
        }

        return false;
    }

    void clear()
    {
        mSelections.clear();
    }

    size_t selectionCount() const { return mSelections.size(); }
    const UniqueId64* selectionData() const { return mSelections.data(); }

    UniqueId64 operator[](size_t index) const
    {
        assert(index >= 0 && index < mSelections.size());
        return mSelections[index];
    }

    std::vector<UniqueId64>::iterator begin() { return mSelections.begin(); }
    std::vector<UniqueId64>::const_iterator begin() const { return mSelections.begin(); }
    std::vector<UniqueId64>::iterator end() { return mSelections.end(); }
    std::vector<UniqueId64>::const_iterator end() const { return mSelections.end(); }

private:
    std::vector<UniqueId64> mSelections;
};

// TODO: Just use std::vector
// Why not just use std::vector and std::find... wrap it in mutex...
struct ContentBrowserItemList
{
    static constexpr size_t InvalidItem = std::numeric_limits<size_t>::max();

    std::vector<ContentBrowserItemPtr> Items;

    std::vector<ContentBrowserItemPtr>::iterator begin() { return Items.begin(); }
    std::vector<ContentBrowserItemPtr>::iterator end() { return Items.end(); }
    std::vector<ContentBrowserItemPtr>::const_iterator begin() const { return Items.begin(); }
    std::vector<ContentBrowserItemPtr>::const_iterator end() const { return Items.end(); }

    ContentBrowserItemPtr& operator[](size_t index) { return Items[index]; }
    const ContentBrowserItemPtr& operator[](size_t index) const { return Items[index]; }

    ContentBrowserItemList() = default;

    ContentBrowserItemList(const ContentBrowserItemList& other)
        : Items(other.Items)
    {
    }

    ContentBrowserItemList& operator=(const ContentBrowserItemList& other)
    {
        Items = other.Items;
        return *this;
    }

    void clear()
    {
        std::scoped_lock<std::mutex> lock(m_Mutex);
        Items.clear();
    }

    void erase(UniqueId64 handle)
    {
        size_t index = findItem(handle);
        if (index == InvalidItem)
            return;

        std::scoped_lock<std::mutex> lock(m_Mutex);
        auto it = Items.begin() + index;
        Items.erase(it);
    }

    size_t findItem(UniqueId64 handle)
    {
        if (Items.size() == 0)
            return InvalidItem;

        std::scoped_lock<std::mutex> lock(m_Mutex);
        for (size_t i = 0; i < Items.size(); i++)
        {
            if (Items[i]->GetUUID() == handle)
                return i;
        }

        return InvalidItem;
    }

private:
    std::mutex m_Mutex;
};

// Modified impl of StudioCherno/Hazel browser
class ContentBrowserPanel {
public:
    ContentBrowserPanel(std::filesystem::path rootDir);

    bool updateAndRender(f32 elapsedSec, bool* isOpen);
    //virtual void OnEvent(Event& e) override;

    ContentBrowserItemList& GetCurrentItems() { return m_CurrentItems; }

    std::shared_ptr<DirectoryInfo> GetDirectory(const std::filesystem::path& filepath) const;
    const std::filesystem::path& getRootPath() const { return mRootPath; }

public:
    inline static std::mutex s_LockMutex; // ensure only one thread accessing file system content at once
    static ContentBrowserPanel& Get() { return *sInstance; }

    STATIC_EVENT_LISTENER_FUNCS(ContentBrowser, AssetCreated, CONTENT_BROWSER_EVENT_TYPE::AssetCreated, ContentBrowserEvent&);
    STATIC_EVENT_LISTENER_FUNCS(ContentBrowser, AssetDeleted, CONTENT_BROWSER_EVENT_TYPE::AssetDeleted, ContentBrowserEvent&);

private:
    void initEvents();
    UniqueId64 ProcessDirectory(const std::filesystem::path& directoryPath, const std::shared_ptr<DirectoryInfo>& parent);

    void ChangeDirectory(std::shared_ptr<DirectoryInfo>& directory);
    void OnBrowseBack();
    void OnBrowseForward();

    void RenderDirectoryHierarchy(std::shared_ptr<DirectoryInfo>& directory);
    void RenderTopBar(float height);
    void RenderItems();
    void RenderBottomBar(float height);

    void Refresh();
    void RefreshWithoutLock();

    void UpdateInput();

    bool OnKeyPressedEvent(const vui::KeyEvent& e);
    bool OnMouseButtonPressed(const vui::MouseButtonEvent& e);

    void PasteCopiedAssets();

    void ClearSelections();

    void RenderDeleteDialogue();
    //void RenderNewScriptDialogue();
    void RemoveDirectory(std::shared_ptr<DirectoryInfo>& directory, bool removeFromParent = true);

    void UpdateDropArea(const std::shared_ptr<DirectoryInfo>& target);

    void SortItemList();

    ContentBrowserItemList Search(const std::string& query, const std::shared_ptr<DirectoryInfo>& directoryInfo);

    void OnFileSystemChanged(const std::vector<FileSystemChangedEvent>& events);

private:
    // NOTE: This should only be used within the ContentBrowserPanel!
    //		 For creating a new asset outside the content browser, use AssetManager::CreateNewAsset!
    template<typename T, typename... Args>
    AssetDescriptor CreateAsset(const std::string& filename, AssetType type) {
        return CreateAssetInDirectory<T>(filename, m_CurrentDirectory);
    }

    // NOTE: This should only be used within the ContentBrowserPanel!
    //		 For creating a new asset outside the content browser, use AssetManager::CreateNewAsset!
    template<typename T, typename... Args>
    AssetDescriptor CreateAssetInDirectory(const std::string& filename, std::shared_ptr<DirectoryInfo>& directory, AssetType type) {
        AssetDescriptor desc;
        IAssetRepositoryBase& repo = Services::ResourceManager::ref().getAssetRepository(type);
        std::string_view fn = Utils::getFilename(std::string_view(filename));
        AssetHandlePtr<T> handle = static_unique_pointer_cast<T>(repo.editorTryAddNewAssetBase(StrToken(fn.data(), fn.size())));
        
        auto filepath = FileSystem::getUniqueFileName(mRootPath / directory->FilePath / std::filesystem::path(filename));
        desc = handle->getDescriptor();
        directory->Assets.emplace_back(std::move(handle));

        ContentBrowserEvent evnt;
        evnt.path = filepath;
        evnt.assetDesc = desc;
        dispatchAssetCreated(evnt);

        return desc;
    }

private:

    std::unordered_map<AssetType, VGTexture> m_AssetIconMap;

    ContentBrowserItemList m_CurrentItems;
    std::filesystem::path mRootPath;
    std::shared_ptr<DirectoryInfo> m_CurrentDirectory;
    std::shared_ptr<DirectoryInfo> m_BaseDirectory;
    std::shared_ptr<DirectoryInfo> m_NextDirectory, m_PreviousDirectory;

    bool m_IsAnyItemHovered = false;

    SelectionStack m_CopiedAssets;

    std::unordered_map<UniqueId64, std::shared_ptr<DirectoryInfo>> m_Directories;

    std::unordered_map<AssetType, std::function<void(const AssetDescriptor&)>> m_ItemActivationCallbacks;
    
    char m_SearchBuffer[MAX_INPUT_BUFFER_LENGTH];

    std::vector<std::shared_ptr<DirectoryInfo>> m_BreadCrumbData;
    bool m_UpdateNavigationPath = false;

    bool m_IsContentBrowserHovered = false;
    bool m_IsContentBrowserFocused = false;

    bool m_ShowAssetType = true;

    inline static ContentBrowserPanel* sInstance = nullptr;

    STATIC_EVENT_DISPATCHER_DEF(ContentBrowser);
};

