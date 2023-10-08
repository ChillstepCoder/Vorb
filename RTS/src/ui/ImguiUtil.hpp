#pragma once

#include <Vorb/ui/imgui/imgui.h>
#include <numeric>  // std::iota

#include "resources/asset/AssetRegistryEntry.h"

namespace ImguiUtil {

    // https://stackoverflow.com/questions/1577475/c-sorting-and-keeping-track-of-indexes
    template <typename T>
    std::vector<size_t> sortIndexes(const std::vector<T>& v, std::function<bool(size_t, size_t)> sortFunction) {

        // initialize original index locations
        std::vector<size_t> idx(v.size());
        std::iota(idx.begin(), idx.end(), 0);

        // sort indexes based on comparing values in v
        // using std::stable_sort instead of std::sort
        // to avoid unnecessary index re-orderings
        // when v contains elements of equal values 
        std::stable_sort(idx.begin(), idx.end(), sortFunction);

        return idx;
    }

    class PopupFilterInterface {
    protected:
        void updateAndRenderFilter() {
            mFilterStatus.resize(mFilterNames.size(), true);
            if (ImGui::InputText("Filter", mFilterBuf, 64)) {
                nString lowerFilter = mFilterBuf;
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                std::string_view filterView(lowerFilter);
                if (lowerFilter[0] == '\0') {
                    for (size_t i = 0; i < mFilterStatus.size(); ++i) {
                        mFilterStatus[i] = true;
                    }
                }
                else {
                    // TODO: Separate by whitespace like unreal
                    for (size_t i = 0; i < mFilterStatus.size(); ++i) {
                        if (mFilterNames[i].find(filterView) == mFilterNames[i].npos) {
                            mFilterStatus[i] = false;
                        }
                        else {
                            mFilterStatus[i] = true;
                        }
                    }
                }
            }
        }
        void setFilter(const std::vector<nString>& filters) {
            mFilterNames = filters;
            for (auto&& filter : mFilterNames) {
                std::transform(filter.begin(), filter.end(), filter.begin(),
                    [](unsigned char c) { return std::tolower(c); });
            }
        }

        std::vector<bool> mFilterStatus;
    private:
        std::vector<nString> mFilterNames;
        char mFilterBuf[64] = {};
    };

    class AssetPopup {
    public:
        AssetPopup(const char* id, const nString& assetName, void* assetPtr) : id(id), assetName(assetName), assetPtr(assetPtr) {
            ImGui::OpenPopup(id);
        }
        virtual ~AssetPopup() {
            ImGui::CloseCurrentPopup();
        }
        void* getAssetPtr() const {
            return assetPtr;
        }
        const nString& getAssetName() const {
            return assetName;
        }
    protected:
        const char* id;
        nString assetName;
        void* assetPtr;
    };

    class ConfirmDeletePopup : public AssetPopup {
    public:
        ConfirmDeletePopup(const nString& assetName, void* assetPtr) : AssetPopup("ConfirmDelete", assetName, assetPtr) {};
        // Return true when closed
        bool updateAndRender() {
            if (ImGui::BeginPopupModal(id, nullptr)) {
                ImGui::Text(("Delete " + assetName + "?").c_str());
                ImGui::Text("This CANNOT be undone!");
                // Your popup content here
                if (ImGui::Button("Yes")) {
                    result = true;
                    ImGui::EndPopup();
                    return true;
                }
                ImGui::SameLine();
                if (ImGui::Button("No")) {
                    ImGui::EndPopup();
                    return true;
                }
                ImGui::EndPopup();
                return false;
            }
            return true;
        }
        bool getResult() const {
            return result;
        }
    protected:
        bool result = false;
    };

    inline int textFieldForceStrtoken(ImGuiInputTextCallbackData* data) {
        char tmpChar = (char)data->EventChar;
        nString tokenStr = StrToken(nString("") + tmpChar).toString();
        data->EventChar = (ImWchar)tokenStr[0];
        return 0;
    }

    class RenameAssetPopup : public AssetPopup {
    public:
        RenameAssetPopup(const nString& assetName, void* assetPtr) : AssetPopup("RenameAsset", assetName, assetPtr) {};
        // Return true when closed
        bool updateAndRender() {
            if (ImGui::BeginPopupModal(id, nullptr)) {
                ImGui::Text(("Rename " + assetName).c_str());
                ImGui::InputText("Name", buffer, MAX_CHARS_IN_STRTOKEN + 1, ImGuiInputTextFlags_CallbackCharFilter, textFieldForceStrtoken);

                if (ImGui::Button("Confirm")) {
                    result = buffer;
                    ImGui::EndPopup();
                    return true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::EndPopup();
                    return true;
                }
                ImGui::EndPopup();
                return false;
            }
            else {
                buffer[0] = '\0';
            }
            return true;
        }

        const nString& getResult() const {
            return result;
        }
    protected:
        nString result = "";
        char buffer[MAX_CHARS_IN_STRTOKEN + 1];
    };

    class CustomSelectorPopup : public AssetPopup, public PopupFilterInterface {
    public:
        CustomSelectorPopup(const std::vector<nString>& names) : mNames(names), AssetPopup("Select", "", nullptr) {
            runSort();
            setFilter(mNames);
        };
        void setNames(const std::vector<nString>& names) {
            mNames = names;
            runSort();
            setFilter(mNames);
        }
        // Return true when closed
        bool updateAndRender() {
            if (ImGui::BeginPopupModal(id, nullptr)) {
                ImGui::Text("Select ");
                updateAndRenderFilter();
                for (size_t i : mSortedIndices) {
                    ImGui::PushID(i);
                    if (mFilterStatus[i]) {
                        if (ImGui::Button("X")) { // TODO: Checkmark image?
                            result = i;
                            ImGui::PopID();
                            ImGui::EndPopup();
                            return true;
                        }
                        ImGui::SameLine();
                        ImGui::Text(mNames[i].c_str());
                    }
                    ImGui::PopID();
                }

                if (ImGui::Button("Cancel")) {
                    ImGui::EndPopup();
                    return true;
                }
                ImGui::EndPopup();
                return false;
            }
            return true;
        }
        size_t getResult() const {
            return result;
        }
        nString getResultName() const {
            if (result == UINT32_MAX) return "";
            return mNames[result];
        }

        const std::vector<nString>& getNames() const {
            return mNames;
        }
    protected:
        void runSort() {
            mSortedIndices = sortIndexes<nString>(mNames, [&](size_t i1, size_t i2) -> bool {
                const nString& n1 = mNames[i1];
                const nString& n2 = mNames[i2];
                return std::lexicographical_compare(n1.begin(), n1.end(), n2.begin(), n2.end());
            });
        }

        std::vector<nString> mNames;
        std::vector<size_t> mSortedIndices;
        size_t result = UINT32_MAX;
    };

    class AssetSelectorPopup : public AssetPopup, public PopupFilterInterface {
    public:
        AssetSelectorPopup(const std::vector<AssetRegistryEntry>& assets) : mAssets(assets), AssetPopup("AssetSelector", "", nullptr) {
            char nameBuf1[MAX_CHARS_IN_STRTOKEN + 1];
            char nameBuf2[MAX_CHARS_IN_STRTOKEN + 1];
            ui32 size1 = 0;
            ui32 size2 = 0;
            mSortedIndices = sortIndexes<AssetRegistryEntry>(assets, [&](size_t i1, size_t i2) -> bool {
                assets[i1].mName.toString(nameBuf1, &size1);
                assets[i2].mName.toString(nameBuf2, &size2);
                std::string_view n1(nameBuf1, size1);
                std::string_view n2(nameBuf2, size2);
                return std::lexicographical_compare(n1.begin(), n1.end(), n2.begin(), n2.end());
            });
            std::vector<nString> filterNames(mAssets.size());
            for (size_t i = 0; i < mAssets.size(); ++i) {
                filterNames[i] = mAssets[i].mName.toString();
            }
            setFilter(filterNames);
        };
        void setThumbnailFunc(std::function<void(AssetID, f32v2)> func, f32v2 thumbnailSize) {
            mThumbnailSize = thumbnailSize;
            mThumbnailFunc = func;
        }
        // Return true when closed
        bool updateAndRender(f32 maxHeight) {
            constexpr ImGuiTableFlags TABLE_FLAGS =
                ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
                | ImGuiTableFlags_Sortable
                | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
                | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
                | ImGuiTableFlags_SizingFixedFit;

            ImVec2 maxSize(1000.0f, maxHeight); // Example values, adjust as needed
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), maxSize);
            if (ImGui::BeginPopupModal(id, nullptr)) {
                updateAndRenderFilter();
                int colCount = mThumbnailFunc ? 4 : 3;
                if (ImGui::BeginTable("AssetTable", colCount, TABLE_FLAGS, ImVec2(0, 0), 0.0f)) {
                    constexpr f32 FIXED_WIDTH = 75.0f;
                    ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 50.0f);
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH * 2.0f);
                    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
                    if (mThumbnailFunc) {
                        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, mThumbnailSize.x);
                    }
                    ImGui::TableSetupScrollFreeze(1, 1);

                    ImGui::TableHeadersRow();

                    for (size_t i : mSortedIndices) {
                        if (mFilterStatus[i]) {
                            ImGui::PushID(i);
                            ImGui::TableNextRow(ImGuiTableRowFlags_None, mThumbnailSize.y);
                            // Select
                            ImGui::TableSetColumnIndex(0);
                            if (ImGui::Button("Select")) {
                                result = mAssets[i];
                                ImGui::PopID();
                                ImGui::EndTable();
                                ImGui::EndPopup();
                                return true;
                            }
                            // Name
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text(mAssets[i].mName.toString().c_str());
                            // ID
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text(std::to_string(mAssets[i].mID).c_str());
                            // Thumbnail
                            if (mThumbnailFunc) {
                                ImGui::TableSetColumnIndex(3);
                                mThumbnailFunc(mAssets[i].mID, mThumbnailSize);
                            }
                           
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTable();
                }
                
                if (ImGui::Button("Cancel")) {
                    ImGui::EndPopup();
                    return true;
                }
                ImGui::EndPopup();
                return false;
            }
            return true;
        }
        AssetRegistryEntry getResult() const {
            return result;
        }
    protected:
        AssetRegistryEntry result;
        const std::vector<AssetRegistryEntry>& mAssets;
        std::vector<size_t> mSortedIndices;
        std::function<void(AssetID, f32v2)> mThumbnailFunc = nullptr;
        f32v2 mThumbnailSize = f32v2(25.0f);
    };

    inline bool ButtonCenteredOnLine(const char* label, ImVec2 size) {
        ImGuiStyle& style = ImGui::GetStyle();

        float avail = ImGui::GetContentRegionAvail().x;

        float off = (avail - size.x) * 0.5f;
        if (off > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);

        return ImGui::Button(label, size);
    }
}