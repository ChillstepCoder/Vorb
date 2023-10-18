#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <numeric>  // std::iota

#include "ui/editor/ImguiColors.h"
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

    // Scoped utils taken from StudioCherno/Hazel ImGuiUtilities.h
    class ScopedStyle
    {
    public:
        ScopedStyle(const ScopedStyle&) = delete;
        ScopedStyle& operator=(const ScopedStyle&) = delete;
        template<typename T>
        ScopedStyle(ImGuiStyleVar styleVar, T value) { ImGui::PushStyleVar(styleVar, value); }
        ~ScopedStyle() { ImGui::PopStyleVar(); }
    };

    class ScopedColor
    {
    public:
        ScopedColor(const ScopedColor&) = delete;
        ScopedColor& operator=(const ScopedColor&) = delete;
        template<typename T>
        ScopedColor(ImGuiCol colourId, T colour) { ImGui::PushStyleColor(colourId, ImColor(colour).Value); }
        ~ScopedColor() { ImGui::PopStyleColor(); }
    };

    class ScopedFont
    {
    public:
        ScopedFont(const ScopedFont&) = delete;
        ScopedFont& operator=(const ScopedFont&) = delete;
        ScopedFont(ImFont* font) { ImGui::PushFont(font); }
        ~ScopedFont() { ImGui::PopFont(); }
    };

    class ScopedID
    {
    public:
        ScopedID(const ScopedID&) = delete;
        ScopedID& operator=(const ScopedID&) = delete;
        template<typename T>
        ScopedID(T id) { ImGui::PushID(id); }
        ~ScopedID() { ImGui::PopID(); }
    };

    class ScopedColorStack
    {
    public:
        ScopedColorStack(const ScopedColorStack&) = delete;
        ScopedColorStack& operator=(const ScopedColorStack&) = delete;

        template <typename ColorType, typename... OtherColors>
        ScopedColorStack(ImGuiCol firstColorID, ColorType firstColor, OtherColors&& ... otherColorPairs)
            : m_Count((sizeof... (otherColorPairs) / 2) + 1)
        {
            static_assert ((sizeof... (otherColorPairs) & 1u) == 0,
                "ScopedColorStack constructor expects a list of pairs of colour IDs and colours as its arguments");

            PushColor(firstColorID, firstColor, std::forward<OtherColors>(otherColorPairs)...);
        }

        ~ScopedColorStack() { ImGui::PopStyleColor(m_Count); }

    private:
        int m_Count;

        template <typename ColorType, typename... OtherColors>
        void PushColor(ImGuiCol colourID, ColorType colour, OtherColors&& ... otherColorPairs)
        {
            if constexpr (sizeof... (otherColorPairs) == 0)
            {
                ImGui::PushStyleColor(colourID, ImColor(colour).Value);
            }
            else
            {
                ImGui::PushStyleColor(colourID, ImColor(colour).Value);
                PushColor(std::forward<OtherColors>(otherColorPairs)...);
            }
        }
    };

    class ScopedStyleStack
    {
    public:
        ScopedStyleStack(const ScopedStyleStack&) = delete;
        ScopedStyleStack& operator=(const ScopedStyleStack&) = delete;

        template <typename ValueType, typename... OtherStylePairs>
        ScopedStyleStack(ImGuiStyleVar firstStyleVar, ValueType firstValue, OtherStylePairs&& ... otherStylePairs)
            : m_Count((sizeof... (otherStylePairs) / 2) + 1)
        {
            static_assert ((sizeof... (otherStylePairs) & 1u) == 0,
                "ScopedStyleStack constructor expects a list of pairs of colour IDs and colours as its arguments");

            PushStyle(firstStyleVar, firstValue, std::forward<OtherStylePairs>(otherStylePairs)...);
        }

        ~ScopedStyleStack() { ImGui::PopStyleVar(m_Count); }

    private:
        int m_Count;

        template <typename ValueType, typename... OtherStylePairs>
        void PushStyle(ImGuiStyleVar styleVar, ValueType value, OtherStylePairs&& ... otherStylePairs)
        {
            if constexpr (sizeof... (otherStylePairs) == 0)
            {
                ImGui::PushStyleVar(styleVar, value);
            }
            else
            {
                ImGui::PushStyleVar(styleVar, value);
                PushStyle(std::forward<OtherStylePairs>(otherStylePairs)...);
            }
        }
    };


    class ScopedItemFlags
    {
    public:
        ScopedItemFlags(const ScopedItemFlags&) = delete;
        ScopedItemFlags& operator=(const ScopedItemFlags&) = delete;
        ScopedItemFlags(const ImGuiItemFlags flags, const bool enable = true)
        {
            assert(!(flags & ImGuiItemFlags_Disabled), "We shouldn't use ImGuiItemFlags_Disabled! Use ImguUtil::BeginDisabled / ImguUtil::EndDisabled instead. It will handle visuals for you.");
            ImGui::PushItemFlag(flags, enable);
        }
        ~ScopedItemFlags() { ImGui::PopItemFlag(); }
    };

    class ScopedDisable
    {
    public:
        ScopedDisable(const ScopedDisable&) = delete;
        ScopedDisable& operator=(const ScopedDisable&) = delete;
        ScopedDisable(bool disabled = true);
        ~ScopedDisable();
    };

    // The delay won't work on texts, because the timer isn't tracked for them.
    inline bool IsItemHovered(float delayInSeconds = 0.1f, ImGuiHoveredFlags flags = 0)
    {
        return ImGui::IsItemHovered() && GImGui->HoveredIdTimer > delayInSeconds; /*HoveredIdNotActiveTimer*/
    }

    inline void SetTooltip(std::string_view text, float delayInSeconds = 0.1f, bool allowWhenDisabled = true, ImVec2 padding = ImVec2(5, 5))
    {
        if (IsItemHovered(delayInSeconds, allowWhenDisabled ? ImGuiHoveredFlags_AllowWhenDisabled : 0))
        {
            ScopedStyle tooltipPadding(ImGuiStyleVar_WindowPadding, padding);
            ScopedColor textCol(ImGuiCol_Text, ImguiColors::Theme::textBrighter);
            ImGui::SetTooltip(text.data());
        }
    }

    // Check if navigated to current item, e.g. with arrow keys
    inline bool NavigatedTo()
    {
        ImGuiContext& g = *GImGui;
        return g.NavJustMovedToId == g.LastItemData.ID;
    }

    //=========================================================================================
    /// Rectangle

    inline ImRect GetItemRect()
    {
        return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    }

    inline ImRect RectExpanded(const ImRect& rect, float x, float y)
    {
        ImRect result = rect;
        result.Min.x -= x;
        result.Min.y -= y;
        result.Max.x += x;
        result.Max.y += y;
        return result;
    }

    inline ImRect RectOffset(const ImRect& rect, float x, float y)
    {
        ImRect result = rect;
        result.Min.x += x;
        result.Min.y += y;
        result.Max.x += x;
        result.Max.y += y;
        return result;
    }

    inline ImRect RectOffset(const ImRect& rect, ImVec2 xy)
    {
        return RectOffset(rect, xy.x, xy.y);
    }

    //=========================================================================================
    /// Button Image

    inline void DrawButtonImage(VGTexture imageNormal, VGTexture imageHovered, VGTexture imagePressed,
        ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
        ImVec2 rectMin, ImVec2 rectMax)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        if (ImGui::IsItemActive())
            drawList->AddImage(ImTextureID(imagePressed), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintPressed);
        else if (ImGui::IsItemHovered())
            drawList->AddImage(ImTextureID(imageHovered), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintHovered);
        else
            drawList->AddImage(ImTextureID(imageNormal), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintNormal);
    };

    inline void DrawButtonImage(VGTexture& imageNormal, VGTexture imageHovered, VGTexture imagePressed,
        ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
        ImRect rectangle)
    {
        DrawButtonImage(imageNormal, imageHovered, imagePressed, tintNormal, tintHovered, tintPressed, rectangle.Min, rectangle.Max);
    };

    inline void DrawButtonImage(VGTexture image,
        ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
        ImVec2 rectMin, ImVec2 rectMax)
    {
        DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, rectMin, rectMax);
    };

    inline void DrawButtonImage(VGTexture image,
        ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
        ImRect rectangle)
    {
        DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, rectangle.Min, rectangle.Max);
    };


    inline void DrawButtonImage(VGTexture imageNormal, VGTexture imageHovered, VGTexture imagePressed,
        ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed)
    {
        DrawButtonImage(imageNormal, imageHovered, imagePressed, tintNormal, tintHovered, tintPressed, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    };

    inline void DrawButtonImage(VGTexture image,
        ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed)
    {
        DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    };


    //=========================================================================================
    /// Border

    inline void DrawBorder(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColor, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        auto min = rectMin;
        min.x -= thickness;
        min.y -= thickness;
        min.x += offsetX;
        min.y += offsetY;
        auto max = rectMax;
        max.x += thickness;
        max.y += thickness;
        max.x += offsetX;
        max.y += offsetY;

        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(borderColor), 0.0f, 0, thickness);
    };

    inline void DrawBorder(const ImVec4& borderColor, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        DrawBorder(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), borderColor, thickness, offsetX, offsetY);
    };

    inline void DrawBorder(float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        DrawBorder(ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
    };

    inline void DrawBorder(ImVec2 rectMin, ImVec2 rectMax, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        DrawBorder(rectMin, rectMax, ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
    };
    inline void DrawBorder(ImRect rect, float thickness = 1.0f, float rounding = 0.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        auto min = rect.Min;
        min.x -= thickness;
        min.y -= thickness;
        min.x += offsetX;
        min.y += offsetY;
        auto max = rect.Max;
        max.x += thickness;
        max.y += thickness;
        max.x += offsetX;
        max.y += offsetY;

        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Border)), rounding, 0, thickness);
    };

    inline void DrawBorderHorizontal(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColor, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        auto min = rectMin;
        min.y -= thickness;
        min.x += offsetX;
        min.y += offsetY;
        auto max = rectMax;
        max.y += thickness;
        max.x += offsetX;
        max.y += offsetY;

        auto* drawList = ImGui::GetWindowDrawList();
        const auto colour = ImGui::ColorConvertFloat4ToU32(borderColor);
        drawList->AddLine(min, ImVec2(max.x, min.y), colour, thickness);
        drawList->AddLine(ImVec2(min.x, max.y), max, colour, thickness);
    };

    inline void DrawBorderHorizontal(ImVec2 rectMin, ImVec2 rectMax, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        DrawBorderHorizontal(rectMin, rectMax, ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
    };

    inline void DrawBorderHorizontal(const ImVec4& borderColor, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        DrawBorderHorizontal(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), borderColor, thickness, offsetX, offsetY);
    };

    inline void DrawBorderHorizontal(float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        DrawBorderHorizontal(ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
    };

    inline void DrawBorderVertical(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColor, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        auto min = rectMin;
        min.x -= thickness;
        min.x += offsetX;
        min.y += offsetY;
        auto max = rectMax;
        max.x += thickness;
        max.x += offsetX;
        max.y += offsetY;

        auto* drawList = ImGui::GetWindowDrawList();
        const auto colour = ImGui::ColorConvertFloat4ToU32(borderColor);
        drawList->AddLine(min, ImVec2(min.x, max.y), colour, thickness);
        drawList->AddLine(ImVec2(max.x, min.y), max, colour, thickness);
    };

    inline void DrawBorderVertical(const ImVec4& borderColor, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        DrawBorderVertical(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), borderColor, thickness, offsetX, offsetY);
    };

    inline void DrawBorderVertical(float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
    {
        DrawBorderVertical(ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
    };

    inline void DrawItemActivityOutline(float rounding = 0.0f, bool drawWhenInactive = false, ImColor colourWhenActive = ImColor(80, 80, 80))
    {
        auto* drawList = ImGui::GetWindowDrawList();
        const ImRect rect = RectExpanded(GetItemRect(), 1.0f, 1.0f);
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            drawList->AddRect(rect.Min, rect.Max,
                ImColor(60, 60, 60), rounding, 0, 1.5f);
        }
        if (ImGui::IsItemActive())
        {
            drawList->AddRect(rect.Min, rect.Max,
                colourWhenActive, rounding, 0, 1.0f);
        }
        else if (!ImGui::IsItemHovered() && drawWhenInactive)
        {
            drawList->AddRect(rect.Min, rect.Max,
                ImColor(50, 50, 50), rounding, 0, 1.0f);
        }
    };

    inline void ShiftCursorX(float distance) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + distance);
    }
    inline void ShiftCursorY(float distance) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + distance);
    }
    inline void ShiftCursor(float x, float y) {
        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cursor.x + x, cursor.y + y));
    }

    //=========================================================================================
    /// Colors

    static ImColor ColorWithValue(const ImColor& color, float value)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(hue, sat, std::min(value, 1.0f));
    }

    static ImColor ColorWithSaturation(const ImColor& color, float saturation)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(hue, std::min(saturation, 1.0f), val);
    }

    static ImColor ColorWithHue(const ImColor& color, float hue)
    {
        const ImVec4& colRaw = color.Value;
        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, h, s, v);
        return ImColor::HSV(std::min(hue, 1.0f), s, v);
    }

    static ImColor ColorWithAlpha(const ImColor& color, float multiplier)
    {
        ImVec4 colRaw = color.Value;
        colRaw.w = multiplier;
        return colRaw;
    }

    static ImColor ColorWithMultipliedValue(const ImColor& color, float multiplier)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(hue, sat, std::min(val * multiplier, 1.0f));
    }

    static ImColor ColorWithMultipliedSaturation(const ImColor& color, float multiplier)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(hue, std::min(sat * multiplier, 1.0f), val);
    }

    static ImColor ColorWithMultipliedHue(const ImColor& color, float multiplier)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(std::min(hue * multiplier, 1.0f), sat, val);
    }

    static ImColor ColorWithMultipliedAlpha(const ImColor& color, float multiplier)
    {
        ImVec4 colRaw = color.Value;
        colRaw.w *= multiplier;
        return colRaw;
    }
}