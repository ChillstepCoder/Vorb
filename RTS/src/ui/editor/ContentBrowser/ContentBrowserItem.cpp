#include "stdafx.h"
#include "ContentBrowserItem.h"

#include "resources/ResourceManager.h"

#include "ui/editor/Selection/EditorSelectionManager.h"
#include "ui/editor/Settings/EditorSettings.h"
#include "ui/ImguiUtil.hpp"

#include <Vorb/ui/InputDispatcher.h>
#include <imgui.h>
#include <imgui_internal.h>

// We use this PR because whoever made these files did :P - https://github.com/ocornut/imgui/pull/846
// See bottom of PR for link to a branch for this PR - https://github.com/thedmd/imgui/tree/feature/docking-layout-external

static char s_RenameBuffer[MAX_INPUT_BUFFER_LENGTH];

ContentBrowserItem::ContentBrowserItem(ItemType type, UUID uuid, const std::string& name, VGTexture icon)
    : mType(type), mUUID(uuid), mFileName(name), mIcon(icon)
{
    mDisplayName = mFileName;
    if (mFileName.size() > 25)
        mDisplayName = mFileName.substr(0, 25) + "...";
}

void ContentBrowserItem::OnRenderBegin()
{
    ImGui::PushID(&mUUID);
    ImGui::BeginGroup();
}

CBItemActionResult ContentBrowserItem::OnRender()
{
    CBItemActionResult result;

    const auto& editorSettings = EditorSettings::get();
    const float thumbnailSize = editorSettings.contentBrowserThumbnailSize;

    SetDisplayNameFromFileName();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    const float edgeOffset = 4.0f;

    const float textLineHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f + edgeOffset * 2.0f;
    const float infoPanelHeight = std::max(thumbnailSize * 0.5f, textLineHeight);

    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    const ImVec2 thumbBottomRight = { topLeft.x + thumbnailSize, topLeft.y + thumbnailSize };
    const ImVec2 infoTopLeft = { topLeft.x,				 topLeft.y + thumbnailSize };
    const ImVec2 bottomRight = { topLeft.x + thumbnailSize, topLeft.y + thumbnailSize + infoPanelHeight };

    auto drawShadow = [](const ImVec2& topLeft, const ImVec2& bottomRight, bool directory)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        const ImRect itemRect = ImguiUtil::RectOffset(ImRect(topLeft, bottomRight), 1.0f, 1.0f);
        drawList->AddRect(itemRect.Min, itemRect.Max, ImguiColors::Theme::propertyField, 6.0f, directory ? 0 : ImDrawFlags_RoundCornersBottom, 2.0f);
    };

    const bool isFocused = ImGui::IsWindowFocused();

    const bool isSelected = EditorSelectionManager::isSelected(EditorSelectionContext::ContentBrowser, mUUID);

    // Fill background
    //----------------

    if (mType != ItemType::Directory)
    {
        auto* drawList = ImGui::GetWindowDrawList();

        // Draw shadow
        drawShadow(topLeft, bottomRight, false);

        // Draw background
        drawList->AddRectFilled(topLeft, thumbBottomRight, ImguiColors::Theme::backgroundDark);
        drawList->AddRectFilled(infoTopLeft, bottomRight, ImguiColors::Theme::groupHeader, 6.0f, ImDrawFlags_RoundCornersBottom);
    }
    else if (ImGui::ItemHoverable(ImRect(topLeft, bottomRight), ImGui::GetID(&mUUID), ImGuiItemFlags_None) || isSelected)
    {
        // If hovered or selected directory

        // Draw shadow
        drawShadow(topLeft, bottomRight, true);

        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(topLeft, bottomRight, ImguiColors::Theme::groupHeader, 6.0f);
    }


    // Thumbnail
    //==========
    // TODO: replace with actual Asset Thumbnail interface

    ImGui::InvisibleButton("##thumbnailButton", ImVec2{ thumbnailSize, thumbnailSize });
    ImguiUtil::DrawButtonImage(mIcon, IM_COL32(255, 255, 255, 225),
        IM_COL32(255, 255, 255, 255),
        IM_COL32(255, 255, 255, 255),
        ImguiUtil::RectExpanded(ImguiUtil::GetItemRect(), -6.0f, -6.0f));

    // Info Panel
    //-----------

    auto renamingWidget = [&]
    {
        ImGui::SetKeyboardFocusHere();
        ImGui::InputText("##rename", s_RenameBuffer, MAX_INPUT_BUFFER_LENGTH);

        if (ImGui::IsItemDeactivatedAfterEdit() || vui::InputDispatcher::key.isKeyPressed(VKEY_KP_ENTER)) {
            Rename(s_RenameBuffer);
            mIsRenaming = false;
            SetDisplayNameFromFileName();
            result.Set(ContentBrowserAction::Renamed, true);
        }
    };

    ImguiUtil::ShiftCursor(edgeOffset, edgeOffset);
    if (mType == ItemType::Directory)
    {
        ImGui::BeginVertical((std::string("InfoPanel") + mDisplayName).c_str(), ImVec2(thumbnailSize - edgeOffset * 2.0f, infoPanelHeight - edgeOffset));
        {
            // Centre align directory name
            ImGui::BeginHorizontal(mFileName.c_str(), ImVec2(thumbnailSize - 2.0f, 0.0f));
            ImGui::Spring();
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (thumbnailSize - edgeOffset * 3.0f));
                const float textWidth = std::min(ImGui::CalcTextSize(mDisplayName.c_str()).x, thumbnailSize);
                if (mIsRenaming)
                {
                    ImGui::SetNextItemWidth(thumbnailSize - edgeOffset * 3.0f);
                    renamingWidget();
                }
                else
                {
                    ImGui::SetNextItemWidth(textWidth);
                    ImGui::Text(mDisplayName.c_str());
                }
                ImGui::PopTextWrapPos();
            }
            ImGui::Spring();
            ImGui::EndHorizontal();

            ImGui::Spring();
        }
        ImGui::EndVertical();
    }
    else
    {
        ImGui::BeginVertical((std::string("InfoPanel") + mDisplayName).c_str(), ImVec2(thumbnailSize - edgeOffset * 3.0f, infoPanelHeight - edgeOffset));
        {
            ImGui::BeginHorizontal("label", ImVec2(0.0f, 0.0f));

            ImGui::SuspendLayout();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (thumbnailSize - edgeOffset * 2.0f));
            if (mIsRenaming)
            {
                ImGui::SetNextItemWidth(thumbnailSize - edgeOffset * 3.0f);
                renamingWidget();
            }
            else
            {
                ImGui::Text(mDisplayName.c_str());
            }
            ImGui::PopTextWrapPos();
            ImGui::ResumeLayout();

            ImGui::Spring();
            ImGui::EndHorizontal();
        }
        ImGui::Spring();
        ImguiUtil::ShiftCursorX(edgeOffset);
        ImGui::BeginHorizontal("assetType", ImVec2(0.0f, 0.0f));
        ImGui::Spring();
        {
            StrToken extension = StrToken(Utils::getExtension(mFileName));
            const IAssetRepositoryBase* assetRepo = Services::ResourceManager::ref().tryGetAssetRepositoryForFileExtension(extension);
            
            ImguiUtil::ScopedColor textColor(ImGuiCol_Text, ImguiColors::Theme::textDarker);
            //if (thumbnailSize < 128)
            //{
            //    ImguiUtil::Fonts::PushFont("ExtraSmall");
            //    if (metadata.Type == AssetType::AnimationController) // because it isn't guaranteed to fit on all thumb sizes
            //        assetType = "ANIMCONTROLLER";
            //}
            //else
            //    ImguiUtil::Fonts::PushFont("Small");
            if (assetRepo) {
                ImGui::TextUnformatted(assetRepo->getAssetTypeDisplayName());
            }
            else {
                ImGui::TextUnformatted("UNKOWN");
            }
            //ImguiUtil::Fonts::PopFont();
        }
        ImGui::EndHorizontal();

        ImGui::Spring(-1.0f, edgeOffset);
        ImGui::EndVertical();
    }
    ImguiUtil::ShiftCursor(-edgeOffset, -edgeOffset);

    if (!mIsRenaming)
    {
        if (vui::InputDispatcher::key.isKeyPressed(VKEY_F2) && isSelected && isFocused)
            StartRenaming();
    }

    ImGui::PopStyleVar(); // ItemSpacing

    // End of the Item Group
    //======================
    ImGui::EndGroup();

    // Draw outline
    //-------------
    if (isSelected || ImGui::IsItemHovered())
    {
        ImRect itemRect = ImguiUtil::GetItemRect();
        auto* drawList = ImGui::GetWindowDrawList();

        if (isSelected)
        {
            const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
            ImColor colTransition = ImguiUtil::ColorWithMultipliedValue(ImguiColors::Theme::selection, 0.8f);

            drawList->AddRect(itemRect.Min, itemRect.Max,
                (mouseDown ? static_cast<ImU32>(colTransition) : ImguiColors::Theme::selection), 6.0f,
                mType == ItemType::Directory ? 0 : ImDrawFlags_RoundCornersBottom, 1.0f);
        }
        else // isHovered
        {
            if (mType != ItemType::Directory)
            {
                drawList->AddRect(itemRect.Min, itemRect.Max,
                    ImguiColors::Theme::muted, 6.0f,
                    ImDrawFlags_RoundCornersBottom, 1.0f);
            }
        }
    }


    // Mouse Events handling
    //======================

    UpdateDrop(result);

    bool dragging = false;
    if (dragging = ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        mIsDragging = true;

        const auto& selectionStack = EditorSelectionManager::getSelections(EditorSelectionContext::ContentBrowser);
        if (!EditorSelectionManager::isSelected(EditorSelectionContext::ContentBrowser, mUUID))
            result.Set(ContentBrowserAction::ClearSelections, true);

        auto& currentItems = ContentBrowserPanel::Get().GetCurrentItems();

        if (selectionStack.size() > 0)
        {
            for (const auto& selectedItemHandles : selectionStack)
            {
                size_t index = currentItems.FindItem(selectedItemHandles);
                if (index == ContentBrowserItemList::InvalidItem)
                    continue;

                const auto& item = currentItems[index];
                ImguiUtil::Image(item->GetIcon(), ImVec2(20, 20));
                ImGui::SameLine();
                const auto& name = item->GetName();
                ImGui::TextUnformatted(name.c_str());
            }

            ImGui::SetDragDropPayload("asset_payload", selectionStack.data(), sizeof(AssetHandle) * selectionStack.size());
        }

        result.Set(ContentBrowserAction::Selected, true);
        ImGui::EndDragDropSource();
    }

    if (ImGui::IsItemHovered())
    {
        result.Set(ContentBrowserAction::Hovered, true);

        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !mIsRenaming)
        {
            result.Set(ContentBrowserAction::Activated, true);
        }
        else
        {
            bool action = SelectionManager::GetSelectionCount(EditorSelectionContext::ContentBrowser) > 1 ? ImGui::IsMouseReleased(ImGuiMouseButton_Left) : ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            const bool isSelected = SelectionManager::IsSelected(EditorSelectionContext::ContentBrowser, mUUID);
            bool skipBecauseDragging = mIsDragging && isSelected;
            if (action && !skipBecauseDragging)
            {
                if (isSelected && Input::IsKeyDown(KeyCode::LeftControl) && !mJustSelected)
                {
                    result.Set(ContentBrowserAction::Deselected, true);
                }

                if (mJustSelected)
                    mJustSelected = false;

                if (!isSelected)
                {
                    result.Set(ContentBrowserAction::Selected, true);
                    mJustSelected = true;
                }

                if (!Input::IsKeyDown(KeyCode::LeftControl) && !Input::IsKeyDown(KeyCode::LeftShift) && mJustSelected)
                    result.Set(ContentBrowserAction::ClearSelections, true);

                if (Input::IsKeyDown(KeyCode::LeftShift))
                    result.Set(ContentBrowserAction::SelectToHere, true);
            }
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    if (ImGui::BeginPopupContextItem("CBItemContextMenu"))
    {
        result.Set(ContentBrowserAction::Selected, true);
        OnContextMenuOpen(result);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    mIsDragging = dragging;

    return result;
}

void ContentBrowserItem::OnRenderEnd()
{
    ImGui::PopID();
    ImGui::NextColumn();
}

void ContentBrowserItem::StartRenaming() {
    if (mIsRenaming)
        return;

    memset(s_RenameBuffer, 0, MAX_INPUT_BUFFER_LENGTH);
    memcpy(s_RenameBuffer, mFileName.c_str(), mFileName.size());
    mIsRenaming = true;
}

void ContentBrowserItem::StopRenaming() {
    mIsRenaming = false;
    SetDisplayNameFromFileName();
    memset(s_RenameBuffer, 0, MAX_INPUT_BUFFER_LENGTH);
}

void ContentBrowserItem::Rename(const nString& newName) {
    OnRenamed(newName);
}

void ContentBrowserItem::SetDisplayNameFromFileName()
{
    const auto& editorSettings = EditorSettings::get();
    const float thumbnailSize = editorSettings.contentBrowserThumbnailSize;

    int maxCharacters = 0.00152587f * (thumbnailSize * thumbnailSize); // 0.00152587f is a magic number that is gained from graphing this equation in desmos and setting the y=25 at x=128

    if (mFileName.size() > maxCharacters)
        mDisplayName = mFileName.substr(0, maxCharacters) + " ...";
    else
        mDisplayName = mFileName;
}

void ContentBrowserItem::OnContextMenuOpen(CBItemActionResult& actionResult)
{
    if (ImGui::MenuItem("Reload"))
        actionResult.Set(ContentBrowserAction::Reload, true);

    if (SelectionManager::GetSelectionCount(EditorSelectionContext::ContentBrowser) == 1 && ImGui::MenuItem("Rename"))
        actionResult.Set(ContentBrowserAction::StartRenaming, true);

    if (ImGui::MenuItem("Copy"))
        actionResult.Set(ContentBrowserAction::Copy, true);

    if (ImGui::MenuItem("Duplicate"))
        actionResult.Set(ContentBrowserAction::Duplicate, true);

    if (ImGui::MenuItem("Delete"))
        actionResult.Set(ContentBrowserAction::OpenDeleteDialogue, true);

    ImGui::Separator();

    if (ImGui::MenuItem("Show In Explorer"))
        actionResult.Set(ContentBrowserAction::ShowInExplorer, true);

    if (ImGui::MenuItem("Open Externally"))
        actionResult.Set(ContentBrowserAction::OpenExternal, true);

    RenderCustomContextItems();
}

ContentBrowserDirectory::ContentBrowserDirectory(const Ref<DirectoryInfo>& directoryInfo)
    : ContentBrowserItem(ContentBrowserItem::ItemType::Directory, directoryInfo->Handle, directoryInfo->FilePath.filename().string(), EditorResources::FolderIcon), m_DirectoryInfo(directoryInfo)
{
}

ContentBrowserDirectory::~ContentBrowserDirectory()
{
}

void ContentBrowserDirectory::OnRenamed(const std::string& newName)
{
    auto target = Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath;
    auto destination = Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath.parent_path() / newName;

    if (Utils::toLower(newName) == Utils::toLower(target.filename().string()))
    {
        auto tmp = Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath.parent_path() / "TempDir";
        FileSystem::Rename(target, tmp);
        target = tmp;
    }

    if (!FileSystem::Rename(target, destination))
    {
        HZ_CORE_ERROR("Couldn't rename {0} to {1}!", m_DirectoryInfo->FilePath.filename().string(), newName);
    }
}

void ContentBrowserDirectory::UpdateDrop(CBItemActionResult& actionResult)
{
    if (SelectionManager::IsSelected(EditorSelectionContext::ContentBrowser, mUUID))
        return;

    if (ImGui::BeginDragDropTarget())
    {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("asset_payload");

        if (payload)
        {
            auto& currentItems = ContentBrowserPanel::Get().GetCurrentItems();
            uint32_t count = payload->DataSize / sizeof(AssetHandle);

            for (uint32_t i = 0; i < count; i++)
            {
                AssetHandle assetHandle = *(((AssetHandle*)payload->Data) + i);
                size_t index = currentItems.FindItem(assetHandle);
                if (index != ContentBrowserItemList::InvalidItem)
                {
                    if (currentItems[index]->Move(m_DirectoryInfo->FilePath))
                    {
                        actionResult.Set(ContentBrowserAction::Refresh, true);
                        currentItems.erase(assetHandle);
                    }
                }
            }
        }

        ImGui::EndDragDropTarget();
    }
}

void ContentBrowserDirectory::Delete()
{
    bool deleted = FileSystem::DeleteFile(Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath);
    if (!deleted)
    {
        HZ_CORE_ERROR("Failed to delete folder {0}", m_DirectoryInfo->FilePath);
        return;
    }

    for (auto asset : m_DirectoryInfo->Assets)
        Project::GetEditorAssetManager()->OnAssetDeleted(asset);
}

bool ContentBrowserDirectory::Move(const std::filesystem::path& destination)
{
    bool wasMoved = FileSystem::MoveFile(Project::GetActive()->GetAssetDirectory() / m_DirectoryInfo->FilePath, Project::GetActive()->GetAssetDirectory() / destination);
    if (!wasMoved)
        return false;

    return true;
}

ContentBrowserAsset::ContentBrowserAsset(const AssetMetadata& assetInfo, const Ref<Texture2D>& icon)
    : ContentBrowserItem(ContentBrowserItem::ItemType::Asset, assetInfo.Handle, assetInfo.FilePath.stem().string(), icon), m_AssetInfo(assetInfo)
{
}

ContentBrowserAsset::~ContentBrowserAsset()
{

}

void ContentBrowserAsset::Delete()
{
    auto filepath = Project::GetEditorAssetManager()->GetFileSystemPath(m_AssetInfo);
    bool deleted = FileSystem::DeleteFile(filepath);
    if (!deleted)
    {
        HZ_CORE_ERROR("Couldn't delete {0}", m_AssetInfo.FilePath);
        return;
    }

    auto& currentDirectory = ContentBrowserPanel::Get().GetDirectory(m_AssetInfo.FilePath.parent_path());
    currentDirectory->Assets.erase(std::remove(currentDirectory->Assets.begin(), currentDirectory->Assets.end(), m_AssetInfo.Handle), currentDirectory->Assets.end());

    Project::GetEditorAssetManager()->OnAssetDeleted(m_AssetInfo.Handle);
}

bool ContentBrowserAsset::Move(const std::filesystem::path& destination)
{
    auto filepath = Project::GetEditorAssetManager()->GetFileSystemPath(m_AssetInfo);
    bool wasMoved = FileSystem::MoveFile(filepath, Project::GetActive()->GetAssetDirectory() / destination);
    if (!wasMoved)
    {
        HZ_CORE_ERROR("Couldn't move {0} to {1}", m_AssetInfo.FilePath, destination);
        return false;
    }

    Project::GetEditorAssetManager()->OnAssetRenamed(m_AssetInfo.Handle, destination / filepath.filename());
    return true;
}

void ContentBrowserAsset::OnRenamed(const std::string& newName)
{
    FileSystem::SkipNextFileSystemChange();

    auto filepath = Project::GetEditorAssetManager()->GetFileSystemPath(m_AssetInfo);
    const std::string extension = filepath.extension().string();
    std::filesystem::path newFilepath = fmt::format("{0}\\{1}{2}", filepath.parent_path().string(), newName, extension);

    std::string targetName = fmt::format("{0}{1}", newName, extension);
    if (Utils::toLower(targetName) == Utils::toLower(filepath.filename().string()))
    {
        FileSystem::RenameFilename(filepath, "temp-rename");
        filepath = fmt::format("{0}\\temp-rename{1}", filepath.parent_path().string(), extension);
    }

    FileSystem::SkipNextFileSystemChange();

    if (FileSystem::RenameFilename(filepath, newName))
    {
        // Update AssetManager with new name
        auto& metadata = Project::GetEditorAssetManager()->GetMetadata(m_AssetInfo.Handle);
        Project::GetEditorAssetManager()->OnAssetRenamed(m_AssetInfo.Handle, newFilepath);
    }
    else
    {
        HZ_CORE_ERROR("Couldn't rename {0} to {1}!", filepath.filename().string(), newName);
    }
}
