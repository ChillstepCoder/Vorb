#include "stdafx.h"
#include "ContentBrowserPanel.h"

#include "filesystem/FileSystem.h"

#include "ui/ImguiUtil.hpp"

//  TODO: Try ImguiInternal.h
#include <imgui.h>
#include <imgui_internal.h>


// Modified impl of StudioCherno/Hazel browser
ContentBrowserPanel::ContentBrowserPanel(std::filesystem::path rootDir) : mRootDirectory(rootDir) {

}

bool ContentBrowserPanel::updateAndRender(f32 elapsedSec, bool* isOpen) {
    //m_IsContentBrowserHovered = false;
    //m_IsContentBrowserFocused = false;
    if (ImGui::Begin("Content Browser", isOpen, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
    {
       // m_IsContentBrowserHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        //m_IsContentBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

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
                ImguiUtil::ScopedColourStack itemBg(ImGuiCol_Header, IM_COL32_DISABLE,
                    ImGuiCol_HeaderActive, IM_COL32_DISABLE);

                if (m_BaseDirectory)
                {
                    // TODO(Yan): can we not sort this every frame?
                    std::vector<Ref<DirectoryInfo>> directories;
                    directories.reserve(m_BaseDirectory->subDirectories.size());
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
                ImRect windowRect = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, 10.0f);
                ImGui::PushClipRect(windowRect.Min, windowRect.Max, false);
                UI::DrawShadowInner(EditorResources::ShadowTexture, 20.0f, windowRect, 1.0f, windowRect.GetHeight() / 4.0f, false, true, false, false);
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
                                std::filesystem::path filepath = FileSystem::getUniqueFileName(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / "New Folder");

                                // NOTE(Peter): For some reason creating new directories through code doesn't trigger a file system change?
                                bool created = FileSystem::createDirectory(filepath);

                                if (created)
                                {
                                    Refresh();
                                    const auto& directoryInfo = GetDirectory(m_CurrentDirectory->FilePath / filepath.filename());
                                    size_t index = m_CurrentItems.FindItem(directoryInfo->Handle);
                                    if (index != ContentBrowserItemList::InvalidItem)
                                    {
                                        SelectionManager::DeselectAll(EditorSelectionContext::ContentBrowser);
                                        SelectionManager::Select(EditorSelectionContext::ContentBrowser, directoryInfo->Handle);
                                        m_CurrentItems[index]->StartRenaming();
                                    }
                                }
                            }

                            ImGui::MenuItem("INSERT ASSETS");

                            ImGui::EndMenu();
                        }

                        if (ImGui::MenuItem("Import"))
                        {
                            panic("TODO: Implement import");
                            /* std::filesystem::path filepath = FileSystem::OpenFileDialog();
                             if (!filepath.empty())
                             {
                                 FileSystem::CopyFile(filepath, Project::GetAssetDirectory() / m_CurrentDirectory->FilePath);
                                 Refresh();
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
                            panic("TODO: Implement show in explorer");
                            //FileSystem::OpenDirectoryInExplorer(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath);
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleVar(); // ItemSpacing

                    const float paddingForOutline = 2.0f;
                    const float scrollBarrOffset = 20.0f + ImGui::GetStyle().ScrollbarSize;
                    float panelWidth = ImGui::GetContentRegionAvail().x - scrollBarrOffset;
                    float cellSize = /*ApplicationSettings::Get().ContentBrowserThumbnailSize + s_Padding*/40.0f + paddingForOutline;
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

                    //RenderDeleteDialogue();
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
