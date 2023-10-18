#pragma once

#include <Vorb/Event.hpp>

#include "filesystem/FileSystem.h"
#include "resources/IAssetRepository.h"
#include "resources/ResourceManager.h"

#include "ui/editor/ContentBrowser/ContentBrowserItem.h"
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

    void copyFrom(const std::vector<UUID>& other)
    {
        mSelections.assign(other.begin(), other.end());
    }

    void select(UUID handle)
    {
        if (isSelected(handle))
            return;

        mSelections.push_back(handle);
    }

    void deselect(UUID handle)
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

    bool isSelected(UUID handle) const
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
    const UUID* selectionData() const { return mSelections.data(); }

    UUID operator[](size_t index) const
    {
        assert(index >= 0 && index < mSelections.size());
        return mSelections[index];
    }

    std::vector<UUID>::iterator begin() { return mSelections.begin(); }
    std::vector<UUID>::const_iterator begin() const { return mSelections.begin(); }
    std::vector<UUID>::iterator end() { return mSelections.end(); }
    std::vector<UUID>::const_iterator end() const { return mSelections.end(); }

private:
    std::vector<UUID> mSelections;
};

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

    void erase(UUID handle)
    {
        size_t index = findItem(handle);
        if (index == InvalidItem)
            return;

        std::scoped_lock<std::mutex> lock(m_Mutex);
        auto it = Items.begin() + index;
        Items.erase(it);
    }

    size_t findItem(UUID handle)
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

    STATIC_EVENT_LISTENER_FUNCS(ContentBrowser, AssetCreated, CONTENT_BROWSER_EVENT_TYPE::AssetCreated, ContentBrowserEvent&);
    STATIC_EVENT_LISTENER_FUNCS(ContentBrowser, AssetDeleted, CONTENT_BROWSER_EVENT_TYPE::AssetDeleted, ContentBrowserEvent&);

private:

    void ChangeDirectory(std::shared_ptr<DirectoryInfo>& directory);
    void OnBrowseBack();
    void OnBrowseForward();

    void RenderDirectoryHierarchy(std::shared_ptr<DirectoryInfo>& directory);
    void RenderTopBar(float height);
    void RenderItems();
    void RenderBottomBar(float height);

    void Refresh();

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
        
        auto filepath = FileSystem::getUniqueFileName(m_BaseDirectory / directory->FilePath / filename);
        desc = handle->getDescriptor();
        directory->assets.emplace_back(std::move(handle));

        ContentBrowserEvent evnt;
        evnt.path = filepath;
        evnt.assetDesc = desc;
        dispatchAssetCreated(evnt);

        return desc;
    }

private:

    std::unordered_map<AssetType, VGTexture> m_AssetIconMap;

    ContentBrowserItemList m_CurrentItems;

    std::shared_ptr<DirectoryInfo> m_CurrentDirectory;
    std::shared_ptr<DirectoryInfo> m_BaseDirectory;
    std::shared_ptr<DirectoryInfo> m_NextDirectory, m_PreviousDirectory;

    bool m_IsAnyItemHovered = false;

    SelectionStack m_CopiedAssets;

    std::unordered_map<UUID, std::shared_ptr<DirectoryInfo>> m_Directories;

    std::unordered_map<AssetType, std::function<void(const AssetDescriptor&)>> m_ItemActivationCallbacks;
    
    char m_SearchBuffer[MAX_INPUT_BUFFER_LENGTH];

    std::vector<std::shared_ptr<DirectoryInfo>> m_BreadCrumbData;
    bool m_UpdateNavigationPath = false;

    bool m_IsContentBrowserHovered = false;
    bool m_IsContentBrowserFocused = false;

    bool m_ShowAssetType = true;

    STATIC_EVENT_DISPATCHER_DEF(ContentBrowser);
};

