#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <numeric>  // std::iota

#include "ui/editor/ImguiColors.h"
#include "resources/asset/AssetMetadata.h"
#include "editor/EditorResources.h"

namespace ImguiUtil {

    inline const char* GenerateID() {
        static uint32_t s_Counter = 0;
        static char s_IDBuffer[16] = "##";
        static char s_LabelIDBuffer[1024];
        _itoa_s(s_Counter++, s_IDBuffer + 2, sizeof(s_IDBuffer) - 2, 16);
        return s_IDBuffer;
    }

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
        AssetSelectorPopup(const std::vector<AssetMetadata>& assets) : mAssets(assets), AssetPopup("AssetSelector", "", nullptr) {
            char nameBuf1[MAX_CHARS_IN_STRTOKEN + 1];
            char nameBuf2[MAX_CHARS_IN_STRTOKEN + 1];
            ui32 size1 = 0;
            ui32 size2 = 0;
            mSortedIndices = sortIndexes<AssetMetadata>(assets, [&](size_t i1, size_t i2) -> bool {
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
                            ImGui::Text(std::to_string(mAssets[i].getId()).c_str());
                            // Thumbnail
                            if (mThumbnailFunc) {
                                ImGui::TableSetColumnIndex(3);
                                mThumbnailFunc(mAssets[i].getId(), mThumbnailSize);
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
        AssetMetadata getResult() const {
            return result;
        }
    protected:
        AssetMetadata result;
        const std::vector<AssetMetadata>& mAssets;
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
            assert(!(flags & ImGuiItemFlags_Disabled) && "We shouldn't use ImGuiItemFlags_Disabled! Use ImguUtil::BeginDisabled / ImguUtil::EndDisabled instead. It will handle visuals for you.");
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
            drawList->AddImage(ImTextureID(imagePressed), rectMin, rectMax, ImVec2(0, 1), ImVec2(1, 0), tintPressed);
        else if (ImGui::IsItemHovered())
            drawList->AddImage(ImTextureID(imageHovered), rectMin, rectMax, ImVec2(0, 1), ImVec2(1, 0), tintHovered);
        else
            drawList->AddImage(ImTextureID(imageNormal), rectMin, rectMax, ImVec2(0, 1), ImVec2(1, 0), tintNormal);
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

    inline ImColor ColorWithValue(const ImColor& color, float value)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(hue, sat, std::min(value, 1.0f));
    }

    inline ImColor ColorWithSaturation(const ImColor& color, float saturation)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(hue, std::min(saturation, 1.0f), val);
    }

    inline ImColor ColorWithHue(const ImColor& color, float hue)
    {
        const ImVec4& colRaw = color.Value;
        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, h, s, v);
        return ImColor::HSV(std::min(hue, 1.0f), s, v);
    }

    inline ImColor ColorWithAlpha(const ImColor& color, float multiplier)
    {
        ImVec4 colRaw = color.Value;
        colRaw.w = multiplier;
        return colRaw;
    }

    inline ImColor ColorWithMultipliedValue(const ImColor& color, float multiplier)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(hue, sat, std::min(val * multiplier, 1.0f));
    }

    inline ImColor ColorWithMultipliedSaturation(const ImColor& color, float multiplier)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(hue, std::min(sat * multiplier, 1.0f), val);
    }

    inline ImColor ColorWithMultipliedHue(const ImColor& color, float multiplier)
    {
        const ImVec4& colRaw = color.Value;
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
        return ImColor::HSV(std::min(hue * multiplier, 1.0f), sat, val);
    }

    inline ImColor ColorWithMultipliedAlpha(const ImColor& color, float multiplier)
    {
        ImVec4 colRaw = color.Value;
        colRaw.w *= multiplier;
        return colRaw;
    }

    //=========================================================================================
    /// Shadows

    inline void DrawShadow(VGTexture shadowImage, int radius, ImVec2 rectMin, ImVec2 rectMax, float alphMultiplier, float lengthStretch,
        bool drawLeft, bool drawRight, bool drawTop, bool drawBottom)
    {
        const float widthOffset = lengthStretch;
        const float alphaTop = std::min(0.25f * alphMultiplier, 1.0f);
        const float alphaSides = std::min(0.30f * alphMultiplier, 1.0f);
        const float alphaBottom = std::min(0.60f * alphMultiplier, 1.0f);
        const auto p1 = rectMin;
        const auto p2 = rectMax;

        ImTextureID textureID = ImTextureID(shadowImage);

        auto* drawList = ImGui::GetWindowDrawList();
        if (drawLeft)
            drawList->AddImage(textureID, { p1.x - widthOffset,  p1.y - radius }, { p2.x + widthOffset, p1.y }, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImColor(0.0f, 0.0f, 0.0f, alphaTop));
        if (drawRight)
            drawList->AddImage(textureID, { p1.x - widthOffset,  p2.y }, { p2.x + widthOffset, p2.y + radius }, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), ImColor(0.0f, 0.0f, 0.0f, alphaBottom));

        if (drawTop)
            drawList->AddImageQuad(textureID, { p1.x - radius, p1.y - widthOffset }, { p1.x, p1.y - widthOffset }, { p1.x, p2.y + widthOffset }, { p1.x - radius, p2.y + widthOffset },
                { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, ImColor(0.0f, 0.0f, 0.0f, alphaSides));
        if (drawBottom)
            drawList->AddImageQuad(textureID, { p2.x, p1.y - widthOffset }, { p2.x + radius, p1.y - widthOffset }, { p2.x + radius, p2.y + widthOffset }, { p2.x, p2.y + widthOffset },
                { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, ImColor(0.0f, 0.0f, 0.0f, alphaSides));
    };

    inline void DrawShadow(VGTexture shadowImage, int radius, ImRect rectangle, float alphMultiplier, float lengthStretch,
        bool drawLeft, bool drawRight, bool drawTop, bool drawBottom)
    {
        DrawShadow(shadowImage, radius, rectangle.Min, rectangle.Max, alphMultiplier, lengthStretch, drawLeft, drawRight, drawTop, drawBottom);
    };


    inline void DrawShadow(VGTexture shadowImage, int radius, float alphMultiplier, float lengthStretch,
        bool drawLeft, bool drawRight, bool drawTop, bool drawBottom)
    {
        DrawShadow(shadowImage, radius, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), alphMultiplier, lengthStretch, drawLeft, drawRight, drawTop, drawBottom);
    };

    inline void DrawShadowInner(VGTexture shadowImage, int radius, ImVec2 rectMin, ImVec2 rectMax, float alpha, float lengthStretch,
        bool drawLeft, bool drawRight, bool drawTop, bool drawBottom)
    {
        const float widthOffset = lengthStretch;
        const float alphaTop = alpha; //std::min(0.25f * alphMultiplier, 1.0f);
        const float alphaSides = alpha; //std::min(0.30f * alphMultiplier, 1.0f);
        const float alphaBottom = alpha; //std::min(0.60f * alphMultiplier, 1.0f);
        const auto p1 = ImVec2(rectMin.x + radius, rectMin.y + radius);
        const auto p2 = ImVec2(rectMax.x - radius, rectMax.y - radius);
        auto* drawList = ImGui::GetWindowDrawList();

        ImTextureID textureID = ImTextureID(shadowImage);

        if (drawTop)
            drawList->AddImage(textureID, { p1.x - widthOffset,  p1.y - radius }, { p2.x + widthOffset, p1.y }, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), ImColor(0.0f, 0.0f, 0.0f, alphaTop));
        if (drawBottom)
            drawList->AddImage(textureID, { p1.x - widthOffset,  p2.y }, { p2.x + widthOffset, p2.y + radius }, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImColor(0.0f, 0.0f, 0.0f, alphaBottom));
        if (drawLeft)
            drawList->AddImageQuad(textureID, { p1.x - radius, p1.y - widthOffset }, { p1.x, p1.y - widthOffset }, { p1.x, p2.y + widthOffset }, { p1.x - radius, p2.y + widthOffset },
                { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, ImColor(0.0f, 0.0f, 0.0f, alphaSides));
        if (drawRight)
            drawList->AddImageQuad(textureID, { p2.x, p1.y - widthOffset }, { p2.x + radius, p1.y - widthOffset }, { p2.x + radius, p2.y + widthOffset }, { p2.x, p2.y + widthOffset },
                { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, ImColor(0.0f, 0.0f, 0.0f, alphaSides));
    };

    inline void DrawShadowInner(VGTexture shadowImage, int radius, ImRect rectangle, float alpha, float lengthStretch,
        bool drawLeft, bool drawRight, bool drawTop, bool drawBottom)
    {
        DrawShadowInner(shadowImage, radius, rectangle.Min, rectangle.Max, alpha, lengthStretch, drawLeft, drawRight, drawTop, drawBottom);
    };


    inline void DrawShadowInner(VGTexture shadowImage, int radius, float alpha, float lengthStretch,
        bool drawLeft, bool drawRight, bool drawTop, bool drawBottom)
    {
        DrawShadowInner(shadowImage, radius, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), alpha, lengthStretch, drawLeft, drawRight, drawTop, drawBottom);
    }

    class Widgets
    {
    public:
        template<uint32_t BuffSize = 256, typename StringType>
        static bool SearchWidget(StringType& searchString, const char* hint = "Search...", bool* grabFocus = nullptr)
        {
            ImGui::PushID(432);

            ShiftCursorY(1.0f);

            const bool layoutSuspended = []
            {
                ImGuiWindow* window = ImGui::GetCurrentWindow();
                if (window->DC.LayoutType)
                {
                    ImGui::SuspendLayout();
                    return true;
                }
                return false;
            }();

            bool modified = false;
            bool searching = false;

            const float areaPosX = ImGui::GetCursorPosX();
            const float framePaddingY = ImGui::GetStyle().FramePadding.y;

            ImguiUtil::ScopedStyle rounding(ImGuiStyleVar_FrameRounding, 3.0f);
            ImguiUtil::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(28.0f, framePaddingY));

            if constexpr (std::is_same<StringType, std::string>::value)
            {
                char searchBuffer[BuffSize]{};
                strcpy_s<BuffSize>(searchBuffer, searchString.c_str());
                if (ImGui::InputText("##Search", searchBuffer, BuffSize))
                {
                    searchString = searchBuffer;
                    modified = true;
                }
                else if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    searchString = searchBuffer;
                    modified = true;
                }

                searching = searchBuffer[0] != 0;
            }
            else
            {
                static_assert(std::is_same<decltype(&searchString[0]), char*>::value,
                    "searchString paramenter must be std::string& or char*");

                if (ImGui::InputText("##Search", searchString, BuffSize))
                {
                    modified = true;
                }
                else if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    modified = true;
                }

                searching = searchString[0] != 0;
            }

            if (grabFocus && *grabFocus)
            {
                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                    && !ImGui::IsAnyItemActive()
                    && !ImGui::IsMouseClicked(0))
                {
                    ImGui::SetKeyboardFocusHere(-1);
                }

                if (ImGui::IsItemFocused())
                    *grabFocus = false;
            }

            ImguiUtil::DrawItemActivityOutline(3.0f, true, ImguiColors::Theme::accent);
            ImGui::SetItemAllowOverlap();

            ImGui::SameLine(areaPosX + 5.0f);

            if (layoutSuspended)
                ImGui::ResumeLayout();

            ImGui::BeginHorizontal("horiz", ImGui::GetItemRectSize());
            const ImVec2 iconSize(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight());

            // Search icon
            {
                const float iconYOffset = framePaddingY - 3.0f;
                ImguiUtil::ShiftCursorY(iconYOffset);
                ImGui::Image((ImTextureID)EditorResources::searchIcon->getLoadedAsset().getTextureHandle(), iconSize, ImVec2(0, 0), ImVec2(1, 1), ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
                ImguiUtil::ShiftCursorY(-iconYOffset);

                // Hint
                if (!searching)
                {
                    ImguiUtil::ShiftCursorY(-framePaddingY + 1.0f);
                    ImguiUtil::ScopedColor text(ImGuiCol_Text, ImguiColors::Theme::textDarker);
                    ImguiUtil::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, framePaddingY));
                    ImGui::TextUnformatted(hint);
                    ImguiUtil::ShiftCursorY(-1.0f);
                }
            }

            ImGui::Spring();

            // Clear icon
            if (searching)
            {
                const float spacingX = 4.0f;
                const float lineHeight = ImGui::GetItemRectSize().y - framePaddingY / 2.0f;

                // TODO: Cant click on this, only hover
                if (ImGui::InvisibleButton("##Clear", ImVec2{lineHeight, lineHeight}))
                {
                    if constexpr (std::is_same<StringType, std::string>::value)
                        searchString.clear();
                    else
                        memset(searchString, 0, BuffSize);

                    modified = true;
                }

                if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

                 ImguiUtil::DrawButtonImage(EditorResources::clearIcon->getLoadedAsset().getTextureHandle(), IM_COL32(160, 160, 160, 200),
                     IM_COL32(170, 170, 170, 255),
                     IM_COL32(160, 160, 160, 150),
                     ImguiUtil::RectExpanded(ImguiUtil::GetItemRect(), -2.0f, -2.0f));

                ImGui::Spring(-1.0f, spacingX * 2.0f);
            }

            ImGui::EndHorizontal();
            ImguiUtil::ShiftCursorY(-1.0f);
            ImGui::PopID();
            return modified;
        }

        static bool AssetSearchPopup(const char* ID, AssetType assetType, UniqueId64& selected, bool* cleared = nullptr, const char* hint = "Search Assets", ImVec2 size = ImVec2{ 250.0f, 350.0f });
        static bool AssetSearchPopup(const char* ID, AssetType assetType, UniqueId64& selected, bool allowMemoryOnlyAssets, bool* cleared = nullptr, const char* hint = "Search Assets", ImVec2 size = ImVec2{ 250.0f, 350.0f });
        static bool AssetSearchPopup(const char* ID, UniqueId64& selected, bool* cleared = nullptr, const char* hint = "Search Assets", ImVec2 size = ImVec2{ 250.0f, 350.0f }, std::initializer_list<AssetType> assetTypes = {});

        static bool OptionsButton()
        {
            const bool clicked = ImGui::InvisibleButton("##options", ImVec2{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() });

            const float spaceAvail = std::min(ImGui::GetItemRectSize().x, ImGui::GetItemRectSize().y);
            const float desiredIconSize = 15.0f;
            const float padding = std::max((spaceAvail - desiredIconSize) / 2.0f, 0.0f);

            constexpr auto buttonColour = ImguiColors::Theme::text;
            const uint8_t value = uint8_t(ImColor(buttonColour).Value.x * 255);
            ImguiUtil::DrawButtonImage(EditorResources::gearIcon->getLoadedAsset().getTextureHandle(), IM_COL32(value, value, value, 200),
                IM_COL32(value, value, value, 255),
                IM_COL32(value, value, value, 150),
                ImguiUtil::RectExpanded(ImguiUtil::GetItemRect(), -padding, -padding));
            return clicked;
        }
    }; // Widgets

    inline bool BeginPopup(const char* str_id, ImGuiWindowFlags flags)
    {
        bool opened = false;
        if (ImGui::BeginPopup(str_id, flags))
        {
            opened = true;
            // Fill background wiht nice gradient
            const float padding = ImGui::GetStyle().WindowBorderSize;
            const ImRect windowRect = ImguiUtil::RectExpanded(ImGui::GetCurrentWindow()->Rect(), -padding, -padding);
            ImGui::PushClipRect(windowRect.Min, windowRect.Max, false);
            const ImColor col1 = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);// Colours::Theme::backgroundPopup;
            const ImColor col2 = ImguiUtil::ColorWithMultipliedValue(col1, 0.8f);
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(windowRect.Min, windowRect.Max, col1, col1, col2, col2);
            ImGui::GetWindowDrawList()->AddRect(windowRect.Min, windowRect.Max, ImguiUtil::ColorWithMultipliedValue(col1, 1.1f));
            ImGui::PopClipRect();

            // Popped in EndPopup()
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 80));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.0f, 1.0f));
        }

        return opened;
    }

    inline void EndPopup()
    {
        ImGui::PopStyleVar(); // WindowPadding;
        ImGui::PopStyleColor(); // HeaderHovered;
        ImGui::EndPopup();
    }

    // MenuBar which allows you to specify its rectangle
    inline bool BeginMenuBar(const ImRect& barRectangle)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;
        /*if (!(window->Flags & ImGuiWindowFlags_MenuBar))
            return false;*/

        IM_ASSERT(!window->DC.MenuBarAppending);
        ImGui::BeginGroup(); // Backup position on layer 0 // FIXME: Misleading to use a group for that backup/restore
        ImGui::PushID("##menubar");

        const ImVec2 padding = window->WindowPadding;

        // We don't clip with current window clipping rectangle as it is already set to the area below. However we clip with window full rect.
        // We remove 1 worth of rounding to Max.x to that text in long menus and small windows don't tend to display over the lower-right rounded area, which looks particularly glitchy.
        ImRect bar_rect = ImguiUtil::RectOffset(barRectangle, 0.0f, padding.y);// window->MenuBarRect();
        ImRect clip_rect(IM_ROUND(ImMax(window->Pos.x, bar_rect.Min.x + window->WindowBorderSize + window->Pos.x - 10.0f)), IM_ROUND(bar_rect.Min.y + window->WindowBorderSize + window->Pos.y),
            IM_ROUND(ImMax(bar_rect.Min.x + window->Pos.x, bar_rect.Max.x - ImMax(window->WindowRounding, window->WindowBorderSize))), IM_ROUND(bar_rect.Max.y + window->Pos.y));

        clip_rect.ClipWith(window->OuterRectClipped);
        ImGui::PushClipRect(clip_rect.Min, clip_rect.Max, false);

        // We overwrite CursorMaxPos because BeginGroup sets it to CursorPos (essentially the .EmitItem hack in EndMenuBar() would need something analogous here, maybe a BeginGroupEx() with flags).
        window->DC.CursorPos = window->DC.CursorMaxPos = ImVec2(bar_rect.Min.x + window->Pos.x, bar_rect.Min.y + window->Pos.y);
        window->DC.LayoutType = ImGuiLayoutType_Horizontal;
        window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;
        window->DC.MenuBarAppending = true;
        ImGui::AlignTextToFramePadding();
        return true;
    }

    inline void EndMenuBar()
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;
        ImGuiContext& g = *GImGui;

        // Nav: When a move request within one of our child menu failed, capture the request to navigate among our siblings.
        if (ImGui::NavMoveRequestButNoResultYet() && (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right) && (g.NavWindow->Flags & ImGuiWindowFlags_ChildMenu))
        {
            // Try to find out if the request is for one of our child menu
            ImGuiWindow* nav_earliest_child = g.NavWindow;
            while (nav_earliest_child->ParentWindow && (nav_earliest_child->ParentWindow->Flags & ImGuiWindowFlags_ChildMenu))
                nav_earliest_child = nav_earliest_child->ParentWindow;
            if (nav_earliest_child->ParentWindow == window && nav_earliest_child->DC.ParentLayoutType == ImGuiLayoutType_Horizontal && (g.NavMoveFlags & ImGuiNavMoveFlags_Forwarded) == 0)
            {
                // To do so we claim focus back, restore NavId and then process the movement request for yet another frame.
                // This involve a one-frame delay which isn't very problematic in this situation. We could remove it by scoring in advance for multiple window (probably not worth bothering)
                const ImGuiNavLayer layer = ImGuiNavLayer_Menu;
                IM_ASSERT(window->DC.NavLayersActiveMaskNext & (1 << layer)); // Sanity check
                ImGui::FocusWindow(window);
                ImGui::SetNavID(window->NavLastIds[layer], layer, 0, window->NavRectRel[layer]);
                g.NavDisableHighlight = true; // Hide highlight for the current frame so we don't see the intermediary selection.
                g.NavDisableMouseHover = g.NavMousePosDirty = true;
                ImGui::NavMoveRequestForward(g.NavMoveDir, g.NavMoveClipDir, g.NavMoveFlags, g.NavMoveScrollFlags); // Repeat
            }
        }

        IM_MSVC_WARNING_SUPPRESS(6011); // Static Analysis false positive "warning C6011: Dereferencing NULL pointer 'window'"
        // IM_ASSERT(window->Flags & ImGuiWindowFlags_MenuBar); // NOTE(Yan): Needs to be commented out because Jay
        IM_ASSERT(window->DC.MenuBarAppending);
        ImGui::PopClipRect();
        ImGui::PopID();
        window->DC.MenuBarOffset.x = window->DC.CursorPos.x - window->Pos.x; // Save horizontal position so next append can reuse it. This is kinda equivalent to a per-layer CursorPos.
        g.GroupStack.back().EmitItem = false;
        ImGui::EndGroup(); // Restore position on layer 0
        window->DC.LayoutType = ImGuiLayoutType_Vertical;
        window->DC.NavLayerCurrent = ImGuiNavLayer_Main;
        window->DC.MenuBarAppending = false;
    }

    extern bool TreeNodeWithIcon(VGTexture icon, ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end, ImColor iconTint = IM_COL32_WHITE);
    extern bool TreeNodeWithIcon(VGTexture icon, const void* ptr_id, ImGuiTreeNodeFlags flags, ImColor iconTint, const char* fmt, ...);
    extern bool TreeNodeWithIcon(VGTexture icon, const char* label, ImGuiTreeNodeFlags flags, ImColor iconTint = IM_COL32_WHITE);
    inline bool TreeNode(const std::string& id, const std::string& label, ImGuiTreeNodeFlags flags = 0, VGTexture icon = 0)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        return ImguiUtil::TreeNodeWithIcon(icon, window->GetID(id.c_str()), flags, label.c_str(), NULL);
    }
}