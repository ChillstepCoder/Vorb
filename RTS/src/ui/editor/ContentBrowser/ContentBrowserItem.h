#pragma once

#include <filesystem>

#include "resources/asset/AssetMetadata.h"

#define MAX_INPUT_BUFFER_LENGTH 128

enum class ContentBrowserAction
{
    None = 0,
    Refresh = BIT(0),
    ClearSelections = BIT(1),
    Selected = BIT(2),
    Deselected = BIT(3),
    Hovered = BIT(4),
    Renamed = BIT(5),
    OpenDeleteDialogue = BIT(6),
    SelectToHere = BIT(7),
    Moved = BIT(8),
    ShowInExplorer = BIT(9),
    OpenExternal = BIT(10),
    Reload = BIT(11),
    Copy = BIT(12),
    Duplicate = BIT(13),
    StartRenaming = BIT(14),
    Activated = BIT(15)
};

struct CBItemActionResult
{
    uint16_t Field = 0;

    void Set(ContentBrowserAction flag, bool value)
    {
        if (value)
            Field |= (uint16_t)flag;
        else
            Field &= ~(uint16_t)flag;
    }

    bool IsSet(ContentBrowserAction flag) const { return (uint16_t)flag & Field; }
};

// Can be a directory or an asset
class ContentBrowserItem
{
public:
    // TODO: Item is redundant. Move out of scope and call ContentBrowserItemType. Superior to ContentBrowserItem::ItemType
    // Ordered by priority in the view
    enum class ItemType : uint16_t {
        Directory, Asset, File
    };
public:
    ContentBrowserItem(ItemType type, UniqueId64 uuid, const std::string& name, VGTexture icon);
    virtual ~ContentBrowserItem() {}

    void OnRenderBegin();
    CBItemActionResult OnRender();
    void OnRenderEnd();

    virtual void Delete() {}
    virtual bool Move(const std::filesystem::path& destination) { return false; }

    UniqueId64 GetUUID() const { return mUUID; }
    ItemType GetType() const { return mType; }
    const std::string& GetName() const { return mFileName; }
    const nString& GetPrevName() const { return mPrevFileName; }

    VGTexture getIcon() const { return mIcon; }

    void StartRenaming();
    void StopRenaming();
    bool IsRenaming() const { return mIsRenaming; }

    void Rename(const nString& newName);
    void SetDisplayNameFromFileName();

protected:
    virtual void OnRenamed(const nString& newName) { mPrevFileName = mFileName; mFileName = newName; }
    virtual void RenderCustomContextItems(CBItemActionResult& actionResult) {}
    virtual void UpdateDrop(CBItemActionResult& actionResult) {}

    void OnContextMenuOpen(CBItemActionResult& actionResult);

protected:
    ItemType mType;
    UniqueId64 mUUID;
    std::string mDisplayName;
    std::string mFileName;
    std::string mPrevFileName;
    VGTexture mIcon;

    bool mIsRenaming = false;
    bool mIsDragging = false;
    bool mJustSelected = false;

private:
    friend class ContentBrowserPanel;
};

struct DirectoryInfo {
    UniqueId64 Handle;
    std::shared_ptr<DirectoryInfo> Parent = nullptr;

    std::filesystem::path FilePath;

    std::map<UniqueId64, nString> Files;
    std::vector<AssetDescriptor> Assets;
    std::map<UniqueId64, std::shared_ptr<DirectoryInfo>> SubDirectories;
};
using DirectoryInfoPtr = std::shared_ptr<DirectoryInfo>;

class ContentBrowserDirectory : public ContentBrowserItem
{
public:
    ContentBrowserDirectory(const std::shared_ptr<DirectoryInfo>& directoryInfo);
    virtual ~ContentBrowserDirectory();

    DirectoryInfoPtr& GetDirectoryInfo() { return m_DirectoryInfo; }

    virtual void Delete() override;
    virtual bool Move(const std::filesystem::path& destination) override;

private:
    virtual void OnRenamed(const std::string& newName) override;
    virtual void UpdateDrop(CBItemActionResult& actionResult) override;

    //void UpdateDirectoryPath(DirectoryInfoPtr directoryInfo, const std::filesystem::path& newParentPath, const std::filesystem::path& newName);

private:
    DirectoryInfoPtr m_DirectoryInfo;
};

class ContentBrowserAsset : public ContentBrowserItem
{
public:
    ContentBrowserAsset(AssetMetadata assetInfo, VGTexture icon);
    virtual ~ContentBrowserAsset();

    const AssetMetadata& GetAssetInfo() const { return m_AssetInfo; }

    virtual void Delete() override;
    virtual bool Move(const std::filesystem::path& destination) override;

private:
    virtual void OnRenamed(const std::string& newName) override;
    virtual void RenderCustomContextItems(CBItemActionResult& actionResult) override;

private:
    AssetMetadata m_AssetInfo;
    std::filesystem::path mPath;
};

using ContentBrowserItemPtr = std::shared_ptr<ContentBrowserItem>;

namespace Utils
{
    static std::string ContentBrowserItemTypeToString(ContentBrowserItem::ItemType type)
    {
        switch (type)
        {
            case ContentBrowserItem::ItemType::Asset: return "Asset";
            case ContentBrowserItem::ItemType::Directory: return "Directory";
        }

        return "Unknown";
    }
}