#pragma once

#include <Vorb/ui/imgui/imgui.h>
#include <numeric>  // std::iota
#include "resources/IAsset.h"

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
        ConfirmDeletePopup(const nString& assetName, void* assetPtr) : AssetPopup("Confirm Delete", assetName, assetPtr) {};
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
            }
            ImGui::EndPopup();
            return false;
        }
        bool getResult() const {
            return result;
        }
    protected:
        bool result = false;
    };

    class RenameAssetPopup : public AssetPopup {
    public:
        RenameAssetPopup(const nString& assetName, void* assetPtr) : AssetPopup("Rename Asset", assetName, assetPtr) {};
        // Return true when closed
        bool updateAndRender() {
            static char buffer[MAX_CHARS_IN_STRTOKEN_WITH_INDEX];
            if (ImGui::BeginPopupModal(id, nullptr)) {
                ImGui::Text(("Rename " + assetName).c_str());
                if (ImGui::InputText("Name", buffer, MAX_CHARS_IN_STRTOKEN_WITH_INDEX)) {
                    // Enforce strtoken
                    nString tokenStr = StrToken(nString(buffer)).toString();
                    memcpy(buffer, tokenStr.data(), tokenStr.size() + 1);
                }
                // Your popup content here
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
            }
            else {
                buffer[0] = '\0';
            }
            ImGui::EndPopup();
            return false;
        }

        const nString& getResult() const {
            return result;
        }
    protected:
        nString result = "";
        char buffer[128];
    };

    class CustomSelectorPopup : public AssetPopup, public PopupFilterInterface {
    public:
        CustomSelectorPopup(const std::vector<nString>& names) : mNames(names), AssetPopup("Select", "", nullptr) {
            mSortedIndices = sortIndexes<nString>(names, [&names](size_t i1, size_t i2) -> bool {
                const nString& n1 = names[i1];
                const nString& n2 = names[i2];
                return std::lexicographical_compare(n1.begin(), n1.end(), n2.begin(), n2.end());
            });
            setFilter(names);
        };
        // Return true when closed
        bool updateAndRender() {
            if (ImGui::BeginPopupModal(id, nullptr)) {
                ImGui::Text("Select ");
                updateAndRenderFilter();
                for (size_t i : mSortedIndices) {
                    if (mFilterStatus[i]) {
                        if (ImGui::Button("X")) {
                            result = i;
                            ImGui::EndPopup();
                            return true;
                        }
                        ImGui::SameLine();
                        ImGui::Text(mNames[i].c_str());
                    }
                }

                if (ImGui::Button("Cancel")) {
                    ImGui::EndPopup();
                    return true;
                }
            }
            ImGui::EndPopup();
            return false;
        }
        size_t getResult() const {
            return result;
        }
    protected:
        std::vector<nString> mNames;
        std::vector<size_t> mSortedIndices;
        size_t result = UINT32_MAX;
    };

    template <IsAssetType T>
    class AssetSelectorPopup : public AssetPopup, public PopupFilterInterface {
    public:
        AssetSelectorPopup(const std::vector<T>& assets) : mAssets(assets), AssetPopup("Asset Selector", "", nullptr) {
            mSortedIndices = sortIndexes<T>(assets, [&assets](size_t i1, size_t i2) -> bool {
                const nString& n1 = assets[i1].getName();
                const nString& n2 = assets[i2].getName();
                return std::lexicographical_compare(n1.begin(), n1.end(), n2.begin(), n2.end());
            });
            std::vector<nString> filterNames(mAssets.size());
            for (size_t i = 0; i < mAssets.size(); ++i) {
                filterNames[i] = mAssets[i].getName();
            }
            setFilter(filterNames);
        };
        // Return true when closed
        bool updateAndRender() {
            if (ImGui::BeginPopupModal(id, nullptr)) {
                updateAndRenderFilter();
                for (size_t i : mSortedIndices) {
                    if (mFilterStatus[i]) {
                        if (ImGui::Button("X")) {
                            result = std::make_pair<T*, AssetID>(const_cast<T*>(&mAssets[i]), mAssets[i].getId());
                            ImGui::EndPopup();
                            return true;
                        }
                        ImGui::SameLine();
                        ImGui::Text(mAssets[i].getName() + " " + nString(mAssets[i].getId()));
                    }
                }
                
                if (ImGui::Button("Cancel")) {
                    ImGui::EndPopup();
                    return true;
                }
            }
            ImGui::EndPopup();
            return false;
        }
        std::pair<T*, AssetID> getResult() const {
            return result;
        }
    protected:
        std::pair<T*, AssetID> result;
        const std::vector<T>& mAssets;
        std::vector<size_t> mSortedIndices;
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