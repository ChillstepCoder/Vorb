#include "stdafx.h"
#include "ContentBrowserPanel.h"

#include "filesystem/FileSystem.h"

#include "ui/ImguiUtil.hpp"
#include "ui/UIContext.h"
#include "ui/editor/EditorRoot.h"

#include "ui/editor/settings/EditorSettings.h"
#include "editor/EditorResources.h"
#include "ui/editor/Selection/EditorSelectionManager.h"

//  TODO: Try ImguiInternal.h
#include <imgui.h>
#include <imgui_internal.h>

static bool s_ActivateSearchWidget = false;

// Modified impl of StudioCherno/Hazel browser
ContentBrowserPanel::ContentBrowserPanel(std::filesystem::path rootDir) : mRootPath(rootDir) {
	assert(!sInstance);
	sInstance = this;

    m_AssetIconMap[AssetType::Tile] = EditorResources::tileIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::ParticleSystem] = EditorResources::psysIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Effect] = EditorResources::effectIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Texture] = EditorResources::pngIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Cubemap] = EditorResources::cubeIcon->getLoadedAsset().getTextureHandle();
    // TODO: Brush conflicts with PNG (TextureDef vs BrushDef)
    m_AssetIconMap[AssetType::Brush] = EditorResources::pngIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Material] = EditorResources::materialIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Rig] = EditorResources::skelIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Animation] = EditorResources::animIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::AnimMachine] = EditorResources::animGraphIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Blendspace1D] = EditorResources::fileIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Model] = EditorResources::meshIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Skill] = EditorResources::skillIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Item] = EditorResources::itemIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Fish] = EditorResources::fishIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::MaterialShader] = EditorResources::shaderIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::TileGrass] = EditorResources::floraIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Biome] = EditorResources::biomeIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::TileDistribution] = EditorResources::fileIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Building] = EditorResources::fileIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::Room] = EditorResources::fileIcon->getLoadedAsset().getTextureHandle();
    m_AssetIconMap[AssetType::NONE] = EditorResources::fileIcon->getLoadedAsset().getTextureHandle();

    static_assert(e_count(AssetType) == 21, "Add icon");
    memset(m_SearchBuffer, 0, MAX_INPUT_BUFFER_LENGTH);

	initEvents();

	Refresh();
}

void ContentBrowserPanel::initEvents() {
	// TODO: Store the returned handles
	vui::InputDispatcher::key.addKeyDownListener([this](const vui::KeyEvent& evnt) {
		if (evnt.wasHandled) return;
		evnt.wasHandled = OnKeyPressedEvent(evnt);
	});
    vui::InputDispatcher::mouse.addButtonDownListener([this](const vui::MouseButtonEvent& evnt) {
        if (evnt.wasHandled) return;
		evnt.wasHandled = OnMouseButtonPressed(evnt);
    });
}

UniqueId64 ContentBrowserPanel::ProcessDirectory(const std::filesystem::path& directoryPath, const std::shared_ptr<DirectoryInfo>& parent)
{
    const auto& directory = GetDirectory(directoryPath);
    // If directory already exists we don't need to process it
    if (directory) {
        return directory->Handle;
    }

    std::shared_ptr<DirectoryInfo> directoryInfo = std::make_shared<DirectoryInfo>();
    directoryInfo->Handle = UniqueId64::Generate();
    directoryInfo->Parent = parent;

    if (directoryPath == mRootPath)
        directoryInfo->FilePath = "";
    else
        directoryInfo->FilePath = std::filesystem::relative(directoryPath, mRootPath);

    // Filesystem iteration
    for (auto entry : std::filesystem::directory_iterator(directoryPath))
    {
        if (entry.is_directory())
        {
            UniqueId64 subdirHandle = ProcessDirectory(entry.path(), directoryInfo);
            directoryInfo->SubDirectories[subdirHandle] = m_Directories[subdirHandle];
        }
        else {
            //std::filesystem::path relativePath = std::filesystem::relative(entry.path(), mRootPath);
            
            ResourceManager& resourceManager = Services::ResourceManager::ref();
            AssetDescriptor desc = resourceManager.registerOrGetRegisteredAsset(entry.path());

            if (desc.isValid()) {
                directoryInfo->Assets.push_back(desc);
			}
			else {
				// Is a random file
				directoryInfo->Files.insert(std::make_pair(UniqueId64::Generate(), Utils::getFilename(entry.path().string())));
			}
        }
    }
    const UniqueId64 uidRv = directoryInfo->Handle;
    m_Directories[uidRv] = std::move(directoryInfo);
    return uidRv;
}

void ContentBrowserPanel::ChangeDirectory(std::shared_ptr<DirectoryInfo>& directory)
{
    if (!directory)
        return;

    m_UpdateNavigationPath = true;

    m_CurrentItems.Items.clear();

    ResourceManager& resourceManager = Services::ResourceManager::ref();

    if (strlen(m_SearchBuffer) == 0)
    {
        for (auto& [subdirHandle, subdir] : directory->SubDirectories) {
            m_CurrentItems.Items.emplace_back(std::make_shared<ContentBrowserDirectory>(subdir));
        }

        for (auto desc : directory->Assets)
        {
            // Asset could have been destroyed
            AssetMetadata metadata = resourceManager.getAssetMetadata(desc);
            if (desc.isValid()) {
                auto&& it = m_AssetIconMap.find(desc.assetType);
				VGTexture icon;
				icon = it != m_AssetIconMap.end() ? it->second : EditorResources::fileIcon->getLoadedAsset().gpuTexture.getHandle();
                m_CurrentItems.Items.emplace_back(std::make_shared<ContentBrowserAsset>(metadata, icon));
            }
        }

		for (auto&& it : directory->Files) {
			m_CurrentItems.Items.emplace_back(std::make_shared<ContentBrowserItem>(ContentBrowserItem::ItemType::File, it.first, it.second, EditorResources::fileIcon->getLoadedAsset().getTextureHandle()));
		}
    }
    else
    {
        m_CurrentItems = Search(m_SearchBuffer, directory);
    }

    SortItemList();

    m_PreviousDirectory = directory;
    m_CurrentDirectory = directory;

    ClearSelections();
}

void ContentBrowserPanel::OnBrowseBack()
{
    m_NextDirectory = m_CurrentDirectory;
    m_PreviousDirectory = m_CurrentDirectory->Parent;
    ChangeDirectory(m_PreviousDirectory);
}

void ContentBrowserPanel::OnBrowseForward()
{
    ChangeDirectory(m_NextDirectory);
}

float s_Padding = 2.0f;
bool s_OpenDeletePopup = false;
bool s_OpenNewScriptPopup = false;

bool ContentBrowserPanel::updateAndRender(f32 elapsedSec, bool* isOpen) {
    m_IsContentBrowserHovered = false;
    m_IsContentBrowserFocused = false;
    if (ImGui::Begin("Content Browser", isOpen, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
    {
        m_IsContentBrowserHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        m_IsContentBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        ImguiUtil::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        ImguiUtil::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

        ImguiUtil::ScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(10.0f, 2.0f));

        ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable
            | ImGuiTableFlags_SizingFixedFit
            | ImGuiTableFlags_BordersInnerV;
        if (ImGui::BeginTable("ContentTable", 2, tableFlags, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupColumn("Outliner", 0, 300.0f);
            ImGui::TableSetupColumn("Directory Structure", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            // Content Outliner

            ImGui::BeginChild("##folders_common");
            {
                ImguiUtil::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                ImguiUtil::ScopedColorStack itemBg(ImGuiCol_Header, IM_COL32_DISABLE,
                    ImGuiCol_HeaderActive, IM_COL32_DISABLE);

                if (m_BaseDirectory)
                {
                    // TODO(Yan): can we not sort this every frame?
                    std::vector<std::shared_ptr<DirectoryInfo>> directories;
                    directories.reserve(m_BaseDirectory->SubDirectories.size());
                    for (auto& [handle, directory] : m_BaseDirectory->SubDirectories)
                        directories.emplace_back(directory);

                    std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b)
                    {
                        return a->FilePath.stem().string() < b->FilePath.stem().string();
                    });

                    for (auto& directory : directories)
                        RenderDirectoryHierarchy(directory);
                }

                // Draw side shadow
                ImRect windowRect = ImguiUtil::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, 10.0f);
                ImGui::PushClipRect(windowRect.Min, windowRect.Max, false);
                ImguiUtil::DrawShadowInner(EditorResources::shadowTexture->getLoadedAsset().getTextureHandle(), 20.0f, windowRect, 1.0f, windowRect.GetHeight() / 4.0f, false, true, false, false);
                ImGui::PopClipRect();
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);

            // Directory Content

            const float topBarHeight = 26.0f;
            const float bottomBarHeight = 32.0f;
            ImGui::BeginChild("##directory_structure", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetWindowHeight() - topBarHeight - bottomBarHeight));
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                RenderTopBar(topBarHeight);
                ImGui::PopStyleVar();

                ImGui::Separator();

                ImGui::BeginChild("Scrolling");
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.35f));

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
                    if (ImGui::BeginPopupContextWindow(0, 1))
                    {
                        if (ImGui::BeginMenu("New"))
                        {
                            if (ImGui::MenuItem("Folder"))
                            {
                                std::filesystem::path filepath = FileSystem::getUniqueFileName(mRootPath / m_CurrentDirectory->FilePath / "New Folder");

                                // NOTE(Peter): For some reason creating new directories through code doesn't trigger a file system change?
                                bool created = FileSystem::createDirectory(filepath);

                                if (created)
                                {
                                    Refresh();
                                    const auto& directoryInfo = GetDirectory(m_CurrentDirectory->FilePath / filepath.filename());
                                    size_t index = m_CurrentItems.findItem(directoryInfo->Handle);
                                    if (index != ContentBrowserItemList::InvalidItem)
                                    {
                                        EditorSelectionManager::deselectAll(EditorSelectionContext::ContentBrowser);
                                        EditorSelectionManager::select(EditorSelectionContext::ContentBrowser, directoryInfo->Handle);
                                        m_CurrentItems[index]->StartRenaming();
                                    }
                                }
                            }

							if (ImGui::BeginMenu("Asset")) {
								for (int i = 0; i < e_count(AssetType); ++i) {
									const AssetType aType = (AssetType)i;
									if (ImGui::MenuItem(ENUM_CSTR(AssetType, aType))) {
										const nString fileName = nString("new_asset.") + ResourceManager::get().getAssetExtension(aType).toString();
										std::filesystem::path filepath = FileSystem::getUniqueFileName(mRootPath / m_CurrentDirectory->FilePath / fileName);
										std::ofstream outfile(filepath);
										if (outfile.is_open()) {
											LOG_DEBUG("Created file {}", filepath.string().c_str());
											outfile.close();
                                            ResourceManager::get().registerOrGetRegisteredAsset(filepath);
                                            Refresh();
										}
									}
                                }
                                ImGui::EndMenu();
							}

                            ImGui::EndMenu();
                        }

                        if (ImGui::MenuItem("Import"))
                        {
                            panic("TODO: Implement import");
                            /* std::filesystem::path filepath = FileSystem::OpenFileDialog();
                             if (!filepath.empty())
                             {
                                 FileSystem::CopyFile(filepath, Project::GetAssetDirectory() / m_CurrentDirectory->FilePath);
                                 refresh();
                             }*/
                        }

                        if (ImGui::MenuItem("Refresh")) {
                            Refresh();
                        }

                        ImGui::MenuItem("TODO CopyPasteDup");
                        //if (ImGui::MenuItem("Copy", "Ctrl+C", nullptr, SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser) > 0))
                        //    m_CopiedAssets.CopyFrom(SelectionManager::GetSelections(SelectionContext::ContentBrowser));

                        //if (ImGui::MenuItem("Paste", "Ctrl+V", nullptr, m_CopiedAssets.SelectionCount() > 0))
                        //    PasteCopiedAssets();

                        //if (ImGui::MenuItem("Duplicate", "Ctrl+D", nullptr, SelectionManager::GetSelectionCount(SelectionContext::ContentBrowser) > 0))
                        //{
                        //    m_CopiedAssets.CopyFrom(SelectionManager::GetSelections(SelectionContext::ContentBrowser));
                        //    PasteCopiedAssets();
                        //}

                        ImGui::Separator();

                        if (ImGui::MenuItem("Show in Explorer")) {
                            FileSystem::openDirectoryInExplorer(mRootPath / m_CurrentDirectory->FilePath);
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleVar(); // ItemSpacing

                    const float paddingForOutline = 2.0f;
                    const float scrollBarrOffset = 20.0f + ImGui::GetStyle().ScrollbarSize;
                    float panelWidth = ImGui::GetContentRegionAvail().x - scrollBarrOffset;
                    const float cellSize = EditorSettings::get().contentBrowserThumbnailSize + paddingForOutline;
                    int columnCount = (int)(panelWidth / cellSize);
                    if (columnCount < 1) columnCount = 1;

                    {
                        const float rowSpacing = 12.0f;
                        ImguiUtil::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(paddingForOutline, rowSpacing));
                        ImGui::Columns(columnCount, 0, false);

                        ImguiUtil::ScopedStyle border(ImGuiStyleVar_FrameBorderSize, 0.0f);
                        ImguiUtil::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                        RenderItems();
                    }

                    if (ImGui::IsWindowFocused() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        UpdateInput();
                    }

                    ImGui::PopStyleColor(2);

                    RenderDeleteDialogue();
                    //RenderNewScriptDialogue();
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();

            RenderBottomBar(bottomBarHeight);

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

std::shared_ptr<DirectoryInfo> ContentBrowserPanel::GetDirectory(const std::filesystem::path& filepath) const
{
    if (filepath.string() == "" || filepath.string() == ".")
        return m_BaseDirectory;

    for (const auto& [handle, directory] : m_Directories)
    {
        if (directory->FilePath == filepath)
            return directory;
    }

    return nullptr;
}

void ContentBrowserPanel::navigateTo(const std::filesystem::path& filepath) {
	std::filesystem::path directoryPath;
	if (FileSystem::isDirectory(filepath)) {
		directoryPath = filepath;
	}
	else {
		directoryPath = filepath.parent_path();
	}
	directoryPath = std::filesystem::relative(directoryPath, mRootPath);

	std::shared_ptr<DirectoryInfo> directory = findDirectory(directoryPath);
	if (directory) {
		ChangeDirectory(directory);
	}
	else {
		LOG_ERROR("Could not find directory {} in content ContentBrowserPanel::navigateTo", filepath.string().c_str());
	}
	
}

std::shared_ptr<DirectoryInfo> findDirectoryRecursive(const std::shared_ptr<DirectoryInfo>& root, const std::filesystem::path& directoryPath) {
    for (const auto& subdir : root->SubDirectories) {
		const DirectoryInfoPtr& dir = subdir.second;
		if (dir->FilePath == directoryPath) {
			return dir;
		}
		DirectoryInfoPtr rv = findDirectoryRecursive(dir, directoryPath);
		if (rv) {
			return rv;
		}
    }
	return nullptr;
}

std::shared_ptr<DirectoryInfo> ContentBrowserPanel::findDirectory(const std::filesystem::path& directoryPath) {
	return findDirectoryRecursive(m_BaseDirectory, directoryPath);
}

void ContentBrowserPanel::RenderDirectoryHierarchy(std::shared_ptr<DirectoryInfo>& directory)
{
	std::string name = directory->FilePath.filename().string();
	std::string id = name + "_TreeNode";
	bool previousState = ImGui::TreeNodeBehaviorIsOpen(ImGui::GetID(id.c_str()));

	// ImGui item height hack
	auto* window = ImGui::GetCurrentWindow();
	window->DC.CurrLineSize.y = 20.0f;
	window->DC.CurrLineTextBaseOffset = 3.0f;
	//---------------------------------------------

	const ImRect itemRect = { window->WorkRect.Min.x, window->DC.CursorPos.y,
							  window->WorkRect.Max.x, window->DC.CursorPos.y + window->DC.CurrLineSize.y };

	const bool isItemClicked = [&itemRect, &id]
	{
		if (ImGui::ItemHoverable(itemRect, ImGui::GetID(id.c_str()), ImGuiItemFlags_None))
		{
			return ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left);
		}
		return false;
	}();

	const bool isWindowFocused = ImGui::IsWindowFocused();


	auto fillWithColor = [&](const ImColor& colour)
	{
		const ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(colour);
		ImGui::GetWindowDrawList()->AddRectFilled(itemRect.Min, itemRect.Max, bgColor);
	};

	// Fill with light selection colour if any of the child entities selected
	auto checkIfAnyDescendantSelected = [&](std::shared_ptr<DirectoryInfo>& directory, auto isAnyDescendantSelected) -> bool
	{
		if (directory->Handle == m_CurrentDirectory->Handle)
			return true;

		if (!directory->SubDirectories.empty())
		{
			for (auto& [childHandle, childDir] : directory->SubDirectories)
			{
				if (isAnyDescendantSelected(childDir, isAnyDescendantSelected))
					return true;
			}
		}

		return false;
	};

	const bool isAnyDescendantSelected = checkIfAnyDescendantSelected(directory, checkIfAnyDescendantSelected);
	const bool isActiveDirectory = directory->Handle == m_CurrentDirectory->Handle;

	ImGuiTreeNodeFlags flags = (isActiveDirectory ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_SpanFullWidth;

	// Fill background
	//----------------
	if (isActiveDirectory || isItemClicked)
	{
		if (isWindowFocused)
			fillWithColor(ImguiColors::Theme::selection);
		else
		{
			const ImColor col = ImguiUtil::ColorWithMultipliedValue(ImguiColors::Theme::selection, 0.8f);
			fillWithColor(ImguiUtil::ColorWithMultipliedSaturation(col, 0.7f));
		}

		ImGui::PushStyleColor(ImGuiCol_Text, ImguiColors::Theme::backgroundDark);
	}
	else if (isAnyDescendantSelected)
	{
		fillWithColor(ImguiColors::Theme::selectionMuted);
	}

	// Tree Node
	//----------

	bool open = ImguiUtil::TreeNode(id, name, flags, EditorResources::folderIcon->getLoadedAsset().getTextureHandle());

	if (isActiveDirectory || isItemClicked)
		ImGui::PopStyleColor();

	// Fixing slight overlap
	ImguiUtil::ShiftCursorY(3.0f);

	// Create Menu
	//------------
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::BeginMenu("New"))
		{
			if (ImGui::MenuItem("Folder"))
			{
				bool created = FileSystem::createDirectory(mRootPath / directory->FilePath / "New Folder");
				if (created)
					Refresh();
			}
			ImGui::EndMenu();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Show in Explorer"))
			FileSystem::openDirectoryInExplorer(mRootPath / directory->FilePath);

		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(); // ItemSpacing

	// Draw children
	//--------------

	if (open)
	{
		std::vector<std::shared_ptr<DirectoryInfo>> directories;
		directories.reserve(m_BaseDirectory->SubDirectories.size());
		for (auto& [handle, directory] : directory->SubDirectories)
			directories.emplace_back(directory);

		std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b)
		{
			return a->FilePath.stem().string() < b->FilePath.stem().string();
		});

		for (auto& child : directories)
			RenderDirectoryHierarchy(child);
	}

	UpdateDropArea(directory);

	if (open != previousState && !isActiveDirectory)
	{
		if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.01f))
			ChangeDirectory(directory);
	}

	if (open)
		ImGui::TreePop();

}

void ContentBrowserPanel::RenderTopBar(float height)
{
	ImGui::BeginChild("##top_bar", ImVec2(0, height));
	ImGui::BeginHorizontal("##top_bar", ImGui::GetWindowSize());
	{
		const float edgeOffset = 4.0f;

		// Navigation buttons
		{
			ImguiUtil::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

			auto contenBrowserButton = [height](const char* labelId, VGTexture icon)
			{
				const ImU32 buttonCol = ImguiColors::Theme::backgroundDark;
				const ImU32 buttonColP = ImguiUtil::ColorWithMultipliedValue(ImguiColors::Theme::backgroundDark, 0.8f);
				ImguiUtil::ScopedColorStack buttonColors(ImGuiCol_Button, buttonCol,
					ImGuiCol_ButtonHovered, buttonCol,
					ImGuiCol_ButtonActive, buttonColP);

				const float iconSize = std::min(24.0f, height);
				const float iconPadding = 3.0f;
				const bool clicked = ImGui::Button(labelId, ImVec2(iconSize, iconSize));
				ImguiUtil::DrawButtonImage(icon, ImguiColors::Theme::textDarker,
					ImguiUtil::ColorWithMultipliedValue(ImguiColors::Theme::textDarker, 1.2f),
					ImguiUtil::ColorWithMultipliedValue(ImguiColors::Theme::textDarker, 0.8f),
					ImguiUtil::RectExpanded(ImguiUtil::GetItemRect(), -iconPadding, -iconPadding));

				return clicked;
			};

			if (contenBrowserButton("##back", EditorResources::backIcon->getLoadedAsset().getTextureHandle()))
			{
				OnBrowseBack();
			}
			ImguiUtil::SetTooltip("Previous directory");

			ImGui::Spring(-1.0f, edgeOffset);

			if (contenBrowserButton("##forward", EditorResources::forwardIcon->getLoadedAsset().getTextureHandle()))
			{
				OnBrowseForward();
			}
			ImguiUtil::SetTooltip("Next directory");

			/*ImGui::Spring(-1.0f, edgeOffset * 2.0f);

			if (contenBrowserButton("##refresh", EditorResources::RefreshIcon))
			{
				Refresh();
			}
			ImguiUtil::SetTooltip("Refresh");*/

			ImGui::Spring(-1.0f, edgeOffset * 2.0f);
		}

		// Search
		{
			ImguiUtil::ShiftCursorY(2.0f);
			ImGui::SetNextItemWidth(200);

			if (s_ActivateSearchWidget)
			{
				ImGui::SetKeyboardFocusHere();
				s_ActivateSearchWidget = false;
			}

			if (ImguiUtil::Widgets::SearchWidget<MAX_INPUT_BUFFER_LENGTH>(m_SearchBuffer))
			{
				if (strlen(m_SearchBuffer) == 0)
				{
					ChangeDirectory(m_CurrentDirectory);
				}
				else
				{
					m_CurrentItems = Search(m_SearchBuffer, m_CurrentDirectory);
					SortItemList();
				}
			}
			ImguiUtil::ShiftCursorY(-2.0f);
		}

		if (m_UpdateNavigationPath)
		{
			m_BreadCrumbData.clear();

			std::shared_ptr<DirectoryInfo> current = m_CurrentDirectory;
			while (current && current->Parent != nullptr)
			{
				m_BreadCrumbData.push_back(current);
				current = current->Parent;
			}

			std::reverse(m_BreadCrumbData.begin(), m_BreadCrumbData.end());
			m_UpdateNavigationPath = false;
		}

		// Breadcrumbs
		{
			ImguiUtil::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);
			ImguiUtil::ScopedColor textColor(ImGuiCol_Text, ImguiColors::Theme::textDarker);

			const std::string assetsDirectoryName = mRootPath.string();
			ImVec2 textSize = ImGui::CalcTextSize(assetsDirectoryName.c_str());
			const float textPadding = ImGui::GetStyle().FramePadding.y;
			if (ImGui::Selectable(assetsDirectoryName.c_str(), false, 0, ImVec2(textSize.x, textSize.y + textPadding)))
			{
				EditorSelectionManager::deselectAll(EditorSelectionContext::ContentBrowser);
				ChangeDirectory(m_BaseDirectory);
			}
			UpdateDropArea(m_BaseDirectory);

			for (auto& directory : m_BreadCrumbData)
			{
				ImGui::Text("/");

				std::string directoryName = directory->FilePath.filename().string();
				ImVec2 textSize = ImGui::CalcTextSize(directoryName.c_str());
				if (ImGui::Selectable(directoryName.c_str(), false, 0, ImVec2(textSize.x, textSize.y + textPadding)))
				{
					EditorSelectionManager::deselectAll(EditorSelectionContext::ContentBrowser);
					ChangeDirectory(directory);
				}

				UpdateDropArea(directory);
			}
		}

		// Settings button
		ImGui::Spring();
		if (ImguiUtil::Widgets::OptionsButton())
		{
			ImGui::OpenPopup("ContentBrowserSettings");
		}
		ImguiUtil::SetTooltip("Content Browser settings");


		if (ImguiUtil::BeginPopup("ContentBrowserSettings", ImGuiWindowFlags_None))
		{
			auto& editorSettings = EditorSettings::get();

			bool saveSettings = ImGui::SliderInt("##thumbnail_size", &editorSettings.contentBrowserThumbnailSize, 64, 512);
			ImGui::SameLine(); ImGui::Text("Thumbnail Size");
			ImguiUtil::SetTooltip("Thumbnail Size");

			if (saveSettings) {
			//	ApplicationSettingsSerializer::SaveSettings();
				LOG_WARN("Must implement ApplicationSettingsSerializer::SaveSettings() to save thumbnail size");
			}

			ImguiUtil::EndPopup();
		}

	}
	ImGui::EndHorizontal();
	ImGui::EndChild();
}

void ContentBrowserPanel::RenderItems()
{
	m_IsAnyItemHovered = false;

	for (auto& item : m_CurrentItems)
	{
		item->OnRenderBegin();

		CBItemActionResult result = item->OnRender();

		item->OnRenderEnd();

		if (result.IsSet(ContentBrowserAction::ClearSelections))
			ClearSelections();

		if (result.IsSet(ContentBrowserAction::Deselected))
			EditorSelectionManager::deselect(EditorSelectionContext::ContentBrowser, item->GetUUID());

		if (result.IsSet(ContentBrowserAction::Selected))
			EditorSelectionManager::select(EditorSelectionContext::ContentBrowser, item->GetUUID());

		if (result.IsSet(ContentBrowserAction::SelectToHere) && EditorSelectionManager::getSelectionCount(EditorSelectionContext::ContentBrowser) == 2)
		{
			size_t firstIndex = m_CurrentItems.findItem(EditorSelectionManager::getSelection(EditorSelectionContext::ContentBrowser, 0));
			size_t lastIndex = m_CurrentItems.findItem(item->GetUUID());

			if (firstIndex > lastIndex)
			{
				size_t temp = firstIndex;
				firstIndex = lastIndex;
				lastIndex = temp;
			}

			for (size_t i = firstIndex; i <= lastIndex; i++)
				EditorSelectionManager::select(EditorSelectionContext::ContentBrowser, m_CurrentItems[i]->GetUUID());
		}

		if (result.IsSet(ContentBrowserAction::StartRenaming))
			item->StartRenaming();

		if (result.IsSet(ContentBrowserAction::Copy))
			m_CopiedAssets.select(item->GetUUID());

        if (item->GetType() == ContentBrowserItem::ItemType::Asset) {
            if (result.IsSet(ContentBrowserAction::Reload)) {
                std::shared_ptr<ContentBrowserAsset> assetItem = static_pointer_cast<ContentBrowserAsset>(item);
				ResourceManager::reloadAsset(assetItem->GetAssetInfo().mDescriptor);
            }
        }

		if (result.IsSet(ContentBrowserAction::OpenDeleteDialogue) && !item->IsRenaming())
		{
			s_OpenDeletePopup = true;
		}

		if (result.IsSet(ContentBrowserAction::ShowInExplorer))
		{
			if (item->GetType() == ContentBrowserItem::ItemType::Directory) {
				FileSystem::showFileInExplorer(mRootPath / m_CurrentDirectory->FilePath / item->GetName());
			}
			else {
				FileSystem::showFileInExplorer(mRootPath / m_CurrentDirectory->FilePath / item->GetName());
			}
		}

		if (result.IsSet(ContentBrowserAction::OpenExternal))
		{
            if (item->GetType() == ContentBrowserItem::ItemType::Directory) {
				FileSystem::openExternally(mRootPath / m_CurrentDirectory->FilePath / item->GetName());
			}
            else {
				FileSystem::openExternally(mRootPath / m_CurrentDirectory->FilePath / item->GetName());
			}
		}

		if (result.IsSet(ContentBrowserAction::Hovered))
			m_IsAnyItemHovered = true;

		//item->OnRenderEnd();

		if (result.IsSet(ContentBrowserAction::Duplicate))
		{
			m_CopiedAssets.select(item->GetUUID());
			PasteCopiedAssets();
			break;
		}

		if (result.IsSet(ContentBrowserAction::Renamed))
		{
			EditorSelectionManager::deselectAll(EditorSelectionContext::ContentBrowser);

			Refresh();
			SortItemList();

			// NOTE (Tim): Calling the next line of code will cause the folder before the renaming to be selected as well, 
			// and so you will have a selection count of 2 whereas only one will be valid, so we will simply not have any selected after a rename.
			//SelectionManager::Select(SelectionContext::ContentBrowser, item->GetID()); 
			break;
		}

		if (result.IsSet(ContentBrowserAction::Activated))
		{
			if (item->GetType() == ContentBrowserItem::ItemType::Directory)
			{
				EditorSelectionManager::deselectAll(EditorSelectionContext::ContentBrowser);
				ChangeDirectory(static_pointer_cast<ContentBrowserDirectory>(item)->GetDirectoryInfo());
				break;
			}
			else if (item->GetType() == ContentBrowserItem::ItemType::Asset)
			{
				std::shared_ptr<ContentBrowserAsset> assetItem = static_pointer_cast<ContentBrowserAsset>(item);
				UIContext::getInstance().getEditorRoot().tryOpenAssetForEdit(assetItem->GetAssetInfo().mDescriptor);
				break;
			}
		}

		if (result.IsSet(ContentBrowserAction::Refresh))
		{
			Refresh();
			break;
		}
	}

	// This is a workaround an issue with ImGui: https://github.com/ocornut/imgui/issues/331
	if (s_OpenDeletePopup)
	{
		ImGui::OpenPopup("Delete");
		s_OpenDeletePopup = false;
	}

	if (s_OpenNewScriptPopup)
	{
		ImGui::OpenPopup("New Script");
		s_OpenNewScriptPopup = false;
	}
}

void ContentBrowserPanel::RenderBottomBar(float height)
{

	ImguiUtil::ScopedStyle childBorderSize(ImGuiStyleVar_ChildBorderSize, 0);
	ImguiUtil::ScopedStyle frameBorderSize(ImGuiStyleVar_FrameBorderSize, 0);
	ImguiUtil::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImguiUtil::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

	ImGui::BeginChild("##bottom_bar", ImVec2(0, height));
	ImGui::BeginHorizontal("##bottom_bar");
	{
		size_t selectionCount = EditorSelectionManager::getSelectionCount(EditorSelectionContext::ContentBrowser);
		if (selectionCount == 1)
		{
			UniqueId64 firstSelection = EditorSelectionManager::getSelection(EditorSelectionContext::ContentBrowser, 0);

			// TODO: Cant we query filepath from the ITEM?
			std::string filepath = "";
			if (m_Directories.find(firstSelection) != m_Directories.end())
			{
				filepath = m_Directories[firstSelection]->FilePath.string();
			}
			else {
				auto&& it = m_CurrentDirectory->Files.find(firstSelection);
				if (it != m_CurrentDirectory->Files.end()) {
					// Random file
					filepath = (m_CurrentDirectory->FilePath / std::filesystem::path(it->second)).string();
				}
				else {
					// Asset
					AssetDescriptor desc(AssetDescriptor::fromUUID(firstSelection));
					if (desc.isValid()) {
						AssetMetadata assetMetadata = Services::ResourceManager::ref().getAssetMetadata(desc);
						filepath = assetMetadata.mFilePath.getString();
					}
				}
			}

			std::replace(filepath.begin(), filepath.end(), '\\', '/');
			ImGui::TextUnformatted(filepath.c_str());
		}
		else if (selectionCount > 1)
		{
			ImGui::Text("%d items selected", selectionCount);
		}
	}
	ImGui::EndHorizontal();
	ImGui::EndChild();
}

void ContentBrowserPanel::Refresh()
{
    m_CurrentItems.clear();
    m_Directories.clear();

    std::shared_ptr<DirectoryInfo> currentDirectory = m_CurrentDirectory;
    UniqueId64 baseDirectoryHandle = ProcessDirectory(mRootPath, nullptr);
    m_BaseDirectory = m_Directories[baseDirectoryHandle];
	if (currentDirectory) {
        m_CurrentDirectory = GetDirectory(currentDirectory->FilePath);
	}

    if (!m_CurrentDirectory)
        m_CurrentDirectory = m_BaseDirectory; // Our current directory was removed

    ChangeDirectory(m_CurrentDirectory);
}

void ContentBrowserPanel::UpdateInput()
{
	if (!m_IsContentBrowserHovered)
		return;

	if ((!m_IsAnyItemHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
		ClearSelections();

	if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)) && EditorSelectionManager::getSelectionCount(EditorSelectionContext::ContentBrowser) > 0)
		ImGui::OpenPopup("Delete");

	if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F5)))
		Refresh();
}

bool ContentBrowserPanel::OnKeyPressedEvent(const vui::KeyEvent& e)
{
    if (!m_IsContentBrowserFocused)
        return false;

    bool handled = false;

    if (e.mod.lCtrl)
    {
        switch (e.keyCode)
        {
            case VKEY_C:
            {
                m_CopiedAssets.copyFrom(EditorSelectionManager::getSelections(EditorSelectionContext::ContentBrowser));
                handled = true;
                break;
            }
            case VKEY_V:
            {
                PasteCopiedAssets();
                handled = true;
                break;
            }
            case VKEY_D:
            {
                m_CopiedAssets.copyFrom(EditorSelectionManager::getSelections(EditorSelectionContext::ContentBrowser));
                PasteCopiedAssets();
                handled = true;
                break;
            }
            case VKEY_F:
            {
                s_ActivateSearchWidget = true;
                break;
            }
        }

        if (e.mod.lShift)
        {
            switch (e.keyCode)
            {
                case VKEY_N:
                {
                    std::filesystem::path filepath = FileSystem::getUniqueFileName(mRootPath / m_CurrentDirectory->FilePath / "New Folder");

                    // NOTE(Peter): For some reason creating new directories through code doesn't trigger a file system change?
                    bool created = FileSystem::createDirectory(filepath);

                    if (created)
                    {
                        Refresh();
                        const auto& directoryInfo = GetDirectory(m_CurrentDirectory->FilePath / filepath.filename());
                        size_t index = m_CurrentItems.findItem(directoryInfo->Handle);
                        if (index != ContentBrowserItemList::InvalidItem)
                        {
                            EditorSelectionManager::deselectAll(EditorSelectionContext::ContentBrowser);
                            EditorSelectionManager::select(EditorSelectionContext::ContentBrowser, directoryInfo->Handle);
                            m_CurrentItems[index]->StartRenaming();
                        }
                    }
                    handled = true;
                }
                break;
            }
        }
    }

    if (e.mod.lAlt || e.mod.rAlt)
    {
        switch (e.keyCode)
        {
            case VKEY_LEFT:
            {
                OnBrowseBack();
                handled = true;
                break;
            }
            case VKEY_RIGHT:
            {
                OnBrowseForward();
                handled = true;
                break;
            }
        }
    }

    if (e.keyCode == VKEY_DELETE && EditorSelectionManager::getSelectionCount(EditorSelectionContext::ContentBrowser) > 0)
    {
        for (const auto& item : m_CurrentItems)
        {
            if (item->IsRenaming())
                return false;
        }

        s_OpenDeletePopup = true;
        handled = true;
    }

    return handled;
}

bool ContentBrowserPanel::OnMouseButtonPressed(const vui::MouseButtonEvent& e)
{
    if (!m_IsContentBrowserFocused)
        return false;

    bool handled = false;
    switch (e.button)
    {
        // Back button
		case vui::MouseButton::X1:
            OnBrowseBack();
            handled = true;
            break;
        // forward button
		case vui::MouseButton::X2:
            OnBrowseForward();
            handled = true;
            break;
    }
    return handled;
}

void ContentBrowserPanel::PasteCopiedAssets()
{
    if (m_CopiedAssets.selectionCount() == 0)
        return;

    auto GetUniquePath = [](const std::filesystem::path& fp)
    {
        int counter = 0;
        auto checkFileName = [&counter, &fp](auto checkFileName) -> std::filesystem::path
        {
            ++counter;
            const std::string counterStr = [&counter] {
                if (counter < 10)
                    return "0" + std::to_string(counter);
                else
                    return std::to_string(counter);
            }();

            std::string basePath = Utils::removeExtension(fp.string()) + "_" + counterStr + fp.extension().string();
            if (std::filesystem::exists(basePath))
                return checkFileName(checkFileName);
            else
                return std::filesystem::path(basePath);
        };

        return checkFileName(checkFileName);
    };

    for (UniqueId64 copiedAsset : m_CopiedAssets)
    {
        size_t assetIndex = m_CurrentItems.findItem(copiedAsset);

        if (assetIndex == ContentBrowserItemList::InvalidItem)
            continue;

        const auto& item = m_CurrentItems[assetIndex];
        auto originalFilePath = mRootPath;

        if (item->GetType() == ContentBrowserItem::ItemType::Asset)
        {
            originalFilePath /= static_pointer_cast<ContentBrowserAsset>(item)->GetAssetInfo().mFilePath.getStdPath();
            auto filepath = GetUniquePath(originalFilePath);
            assert(!std::filesystem::exists(filepath));
            std::filesystem::copy_file(originalFilePath, filepath);
		}
		else if (item->GetType() == ContentBrowserItem::ItemType::File) {
			originalFilePath /= static_pointer_cast<ContentBrowserItem>(item)->GetName();
            auto filepath = GetUniquePath(originalFilePath);
            assert(!std::filesystem::exists(filepath));
            std::filesystem::copy_file(originalFilePath, filepath);
		}
        else
        {
			// Directory
			assert(item->GetType() == ContentBrowserItem::ItemType::Directory);
            originalFilePath /= static_pointer_cast<ContentBrowserDirectory>(item)->GetDirectoryInfo()->FilePath;
            auto filepath = GetUniquePath(originalFilePath);
            assert(!std::filesystem::exists(filepath));
            std::filesystem::copy(originalFilePath, filepath, std::filesystem::copy_options::recursive);
        }
    }

	Refresh();
    EditorSelectionManager::deselectAll();
    m_CopiedAssets.clear();
}

void ContentBrowserPanel::ClearSelections()
{
	std::vector<UniqueId64> selectedItems = EditorSelectionManager::getSelections(EditorSelectionContext::ContentBrowser);
	for (UniqueId64 itemHandle : selectedItems)
	{
		size_t index = m_CurrentItems.findItem(itemHandle);

		if (index == ContentBrowserItemList::InvalidItem)
			continue;

		EditorSelectionManager::deselect(EditorSelectionContext::ContentBrowser, itemHandle);

		if (m_CurrentItems[index]->IsRenaming())
			m_CurrentItems[index]->StopRenaming();
	}
}

static bool s_IsDeletingItems = false;
bool rightButtonHovered = false;
bool leftButtonHovered = false;
void ContentBrowserPanel::RenderDeleteDialogue()
{
	if (ImGui::BeginPopupModal("Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (EditorSelectionManager::getSelectionCount(EditorSelectionContext::ContentBrowser) == 0)
			ImGui::CloseCurrentPopup();

		ImGui::Text("Are you sure you want to delete %d items?", EditorSelectionManager::getSelectionCount(EditorSelectionContext::ContentBrowser));

		const float contentRegionWidth = ImGui::GetContentRegionAvail().x;
		const float buttonWidth = 60.0f;

		if (!rightButtonHovered)
		{
			rightButtonHovered = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow));
			leftButtonHovered = !rightButtonHovered;
		}
		if (!leftButtonHovered)
		{
			leftButtonHovered = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow));
			rightButtonHovered = !leftButtonHovered;
		}

		ImguiUtil::ShiftCursorX(((contentRegionWidth - (buttonWidth * 2.0f)) / 2.0f) - ImGui::GetStyle().ItemSpacing.x);
		if (ImGui::Button("Yes", ImVec2(buttonWidth, 0.0f)) || (rightButtonHovered && vui::InputDispatcher::key.isKeyPressed(VKEY_KP_ENTER)))
		{
			s_IsDeletingItems = true;

			std::vector<AssetMetadata> deletedAssetMetadata;

			auto selectedItems = EditorSelectionManager::getSelections(EditorSelectionContext::ContentBrowser);
			for (UniqueId64 handle : selectedItems)
			{
				size_t index = m_CurrentItems.findItem(handle);
				if (index == ContentBrowserItemList::InvalidItem) {
					continue;
				}
				if (m_CurrentItems[index]->GetType() == ContentBrowserItem::ItemType::Directory) {
					// Directories handled later
					continue;
				}
				{
					AssetMetadata metaData = ResourceManager::get().getAssetMetadata(AssetDescriptor::fromUUID(handle));
					if (metaData.isValid()) {
						deletedAssetMetadata.emplace_back(std::move(metaData));
					}
				}

				m_CurrentItems[index]->Delete();
				m_CurrentItems.erase(handle);
			}

            ContentBrowserEvent evnt;
            for (const auto& deletedMetadata : deletedAssetMetadata) {
				evnt.assetDesc = deletedMetadata.mDescriptor;
				evnt.path = deletedMetadata.mFilePath.getStdPath();
				// TODO: Use m
				dispatchAssetDeleted(evnt);
            }
			
			// Handle directories last so we can ensure all files are deleted in order
			for (UniqueId64 handle : selectedItems)
			{
				if (m_Directories.find(handle) != m_Directories.end())
					RemoveDirectory(m_Directories[handle]);
			}

			EditorSelectionManager::deselectAll(EditorSelectionContext::ContentBrowser);
			Refresh();

			s_IsDeletingItems = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		ImGui::SetItemDefaultFocus();
		if (ImGui::Button("No", ImVec2(buttonWidth, 0.0f)) || (leftButtonHovered && vui::InputDispatcher::key.isKeyPressed(VKEY_KP_ENTER)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}

//void ContentBrowserPanel::RenderNewScriptDialogue()
//{
//	static constexpr size_t MaxClassNameLength = 64 + 1;
//	static constexpr size_t MaxClassNamespaceLength = 64 + 1;
//	static char s_ScriptNameBuffer[MaxClassNameLength]{ 0 };
//	static char s_ScriptNamespaceBuffer[MaxClassNamespaceLength]{ 0 };
//
//	ImguiUtil::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
//
//	ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f));
//    if (ImGui::BeginPopupModal("New Script", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
//    {
//        ImGui::SetNextItemWidth(-1);
//        ImGui::InputTextWithHint("##ScriptNamespace", Project::GetActive()->GetConfig().DefaultNamespace.c_str(), s_ScriptNamespaceBuffer, MaxClassNamespaceLength);
//
//        ImGui::SetNextItemWidth(-1);
//        ImGui::InputTextWithHint("##ScriptName", "Class Name", s_ScriptNameBuffer, MaxClassNameLength);
//
//        ImGui::Separator();
//
//        const bool fileAlreadyExists = FileSystem::Exists(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / (std::string(s_ScriptNameBuffer) + ".cs"));
//        ImGui::BeginDisabled(fileAlreadyExists);
//        if (ImGui::Button("Create"))
//        {
//            if (strlen(s_ScriptNamespaceBuffer) == 0)
//                strcpy(s_ScriptNamespaceBuffer, Project::GetActive()->GetConfig().DefaultNamespace.c_str());
//
//            if (strlen(s_ScriptNameBuffer) > 0)
//            {
//                CreateAsset<ScriptFileAsset>(std::string(s_ScriptNameBuffer) + ".cs", s_ScriptNamespaceBuffer, s_ScriptNameBuffer);
//                ImGui::CloseCurrentPopup();
//            }
//        }
//        ImGui::EndDisabled();
//
//        ImGui::SameLine();
//
//        if (ImGui::Button("Close"))
//            ImGui::CloseCurrentPopup();
//
//        ImGui::EndPopup();
//    }
//
//    if (!ImGui::IsPopupOpen("New Script") && strlen(s_ScriptNameBuffer) > 0)
//    {
//        memset(s_ScriptNameBuffer, 0, MaxClassNameLength);
//        memset(s_ScriptNamespaceBuffer, 0, MaxClassNamespaceLength);
//    }
//}

void ContentBrowserPanel::RemoveDirectory(std::shared_ptr<DirectoryInfo>& directory, bool removeFromParent)
{
	if (directory->Parent && removeFromParent)
	{
		auto& childList = directory->Parent->SubDirectories;
		childList.erase(childList.find(directory->Handle));
	}

	for (auto& [handle, subdir] : directory->SubDirectories)
		RemoveDirectory(subdir, false);

	directory->SubDirectories.clear();
	directory->Assets.clear();

	m_Directories.erase(m_Directories.find(directory->Handle));
}

void ContentBrowserPanel::UpdateDropArea(const std::shared_ptr<DirectoryInfo>& target) {
	if (target && (target->Handle != m_CurrentDirectory->Handle) && ImGui::BeginDragDropTarget()) {
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("asset_payload");
		if (payload) {
			uint32_t count = payload->DataSize / sizeof(UniqueId64);

			for (uint32_t i = 0; i < count; i++)
			{
				UniqueId64 assetHandle = *(((UniqueId64*)payload->Data) + i);
				size_t index = m_CurrentItems.findItem(assetHandle);
				if (index != ContentBrowserItemList::InvalidItem)
				{
					m_CurrentItems[index]->Move(target->FilePath);
					m_CurrentItems.erase(assetHandle);
				}
			}
		}

		ImGui::EndDragDropTarget();
	}
}

void ContentBrowserPanel::SortItemList()
{
	std::sort(m_CurrentItems.begin(), m_CurrentItems.end(), [](const std::shared_ptr<ContentBrowserItem>& item1, const std::shared_ptr<ContentBrowserItem>& item2)
	{
		if (item1->GetType() == item2->GetType())
			return Utils::toLower(item1->GetName()) < Utils::toLower(item2->GetName());

		return (uint16_t)item1->GetType() < (uint16_t)item2->GetType();
	});
}

ContentBrowserItemList ContentBrowserPanel::Search(const std::string& query, const std::shared_ptr<DirectoryInfo>& directoryInfo)
{
	ContentBrowserItemList results;
	std::string queryLowerCase = Utils::toLower(query);

	for (auto& [handle, subdir] : directoryInfo->SubDirectories)
	{
		std::string subdirName = subdir->FilePath.filename().string();
		if (subdirName.find(queryLowerCase) != std::string::npos)
			results.Items.push_back(std::make_shared<ContentBrowserDirectory>(subdir));

		ContentBrowserItemList list = Search(query, subdir);
		results.Items.insert(results.Items.end(), list.Items.begin(), list.Items.end());
	}

	for (auto& assetHandle : directoryInfo->Assets)
	{
		const AssetMetadata asset = ResourceManager::get().getAssetMetadata(assetHandle);
		const std::string filename = Utils::toLower(Utils::getFilename(asset.mFilePath.getString()));

		if (filename.find(queryLowerCase) != std::string::npos) {
			const AssetType assetType = ResourceManager::get().getAssetTypeForFilePath(asset.mFilePath.getStdPath());
			const VGTexture icon = m_AssetIconMap.find(assetType) != m_AssetIconMap.end() ? m_AssetIconMap[assetType] : EditorResources::fileIcon->getLoadedAsset().getTextureHandle();
			results.Items.push_back(std::make_shared<ContentBrowserAsset>(asset, icon));
		}
	}

    for (auto&& it : directoryInfo->Files)
    {
        if (it.second.find(queryLowerCase) != std::string::npos) {
            results.Items.push_back(std::make_shared<ContentBrowserItem>(ContentBrowserItem::ItemType::File, it.first, it.second, EditorResources::fileIcon->getLoadedAsset().getTextureHandle()));
        }
    }

	return results;
}

void ContentBrowserPanel::OnFileSystemChanged(const std::vector<FileSystemChangedEvent>& events)
{
	if (s_IsDeletingItems)
		return;

	Refresh();

	for (const auto& e : events)
	{
		if (e.Action != FileSystemAction::Added)
			continue;

		UniqueId64 handle;

		if (!e.IsDirectory)
		{
			AssetMetadata assetData = ResourceManager::get().tryGetAssetMetadataForPath(e.FilePath);
			if (assetData.isValid()) {
				handle = assetData.getUUID();
			}
		}
		else
		{
			const auto& directoryInfo = GetDirectory(e.FilePath);
			handle = directoryInfo->Handle;
		}

		if (handle == 0)
			continue;

		size_t itemIndex = m_CurrentItems.findItem(handle);

		if (itemIndex == ContentBrowserItemList::InvalidItem)
			continue;

		auto& item = m_CurrentItems[itemIndex];
		EditorSelectionManager::select(EditorSelectionContext::ContentBrowser, handle);
		break;
	}
}