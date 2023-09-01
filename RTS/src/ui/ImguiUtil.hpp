#pragma once

#include <Vorb/ui/imgui/imgui.h>
#include <numeric>  // std::iota
#include "resources/IAsset.h"
namespace ImguiUtil {

    // https://stackoverflow.com/questions/1577475/c-sorting-and-keeping-track-of-indexes
    template <typename T>
    std::vector<size_t> sortIndexes(const std::vector<T>& v, std::function<void(size_t, size_t)> sortFunction) {

        // initialize original index locations
        std::vector<size_t> idx(v.size());
        std::iota(idx.begin(), idx.end(), 0);

        // sort indexes based on comparing values in v
        // using std::stable_sort instead of std::sort
        // to avoid unnecessary index re-orderings
        // when v contains elements of equal values 
        stable_sort(idx.begin(), idx.end(), sortFunction);

        return idx;
    }

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
            static char buffer[128];
            if (ImGui::BeginPopupModal(id, nullptr)) {
                ImGui::Text(("Rename " + assetName).c_str());
                ImGui::InputText("Name", buffer, 128);
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

    template <IsAssetType T>
    class AssetSelectorPopup : public AssetPopup {
    public:
        AssetSelectorPopup(const std::vector<T>& assets) : mAssets(assets), AssetPopup("Asset Selector", "", nullptr) {
            mSortedIndices = sortIndexes<T>(assets, [](size_t i1, size_t i2) { 
                const nString& n1 = assets[i1].getName();
                const nString& n2 = assets[i2].getName();
                return std::lexicographical_compare(n1.begin(), n1.end(), n2.begin(), n2.end());
            });
        };
        // Return true when closed
        bool updateAndRender() {
            if (ImGui::BeginPopupModal(id, nullptr)) {
                for (size_t i : mSortedIndices) {
                    if (ImGui::Button("X")) {
                        result = std::make_pair<T*, AssetID>(const_cast<T*>(&mAssets[i]), mAssets[i].getId());
                        ImGui::EndPopup();
                        return true;
                    }
                    ImGui::SameLine();
                    ImGui::Text(mAssets[i].getName() + " " + nString(mAssets[i].getId()));
                }
                
                ImGui::SameLine();
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