#pragma once

#include "math/Random.h"
#include "ui/imgui_controls/ObjectVector.h"

template <typename T>
class RollTable;

template <typename T>
class RollTableEntry {
public:
    RollTableEntry() = default;
    ~RollTableEntry() = default;

    VORB_NON_COPYABLE_BUT_MOVABLE(RollTableEntry);

    T value;
    f32 weight = 0.0f;
    i32v2 quantityRange = i32v2(0);
    std::unique_ptr<RollTable<T>> subTable = nullptr;
};

template <typename T>
class RollTable {
public:
    RollTable() = default;
    ~RollTable() = default;

    VORB_NON_COPYABLE_BUT_MOVABLE(RollTable);

    struct Result {
        T value;
        i32 quantity;
    };

    void setGuaranteedEntries(std::vector<RollTableEntry<T>>&& entries) {
        mGuaranteedEntries = std::move(entries);
        updateMaxGuaranteedEntries();
    }

    void setRandomEntries(std::vector<RollTableEntry<T>>&& entries) {
        mRandomEntries = std::move(entries);
        updateWeight();
    }

    i32 getRequiredBufferSizeForRollCount(i32 n) const {
        return n + mMaximumGuaranteedEntries;
    }

    // Returns number of drops returned in outputBuffer
    i32 rollN(std::span<Result> outputBuffer, i32 n) const;

    // Serialization
    void ymlWrite(c4::yml::NodeRef& n) const;
    bool ymlRead(c4::yml::ConstNodeRef const& n);

    // Editor
    bool updateAndRenderImgui(const char* label);

protected:
    void displayProbabilityTable(float parentProbability = 100.0f, int depth = 0) const;

    i32 updateMaxGuaranteedEntries() {
        mMaximumGuaranteedEntries = 0;
        for (const auto& entry : mGuaranteedEntries) {
            if (entry.subTable) {
                mMaximumGuaranteedEntries += entry.subTable->updateMaxGuaranteedEntries();
            }
            else {
                ++mMaximumGuaranteedEntries;
            }
        }
        return mMaximumGuaranteedEntries;
    }

    void updateWeight() {
        mTotalWeight = mEmptyWeight;
        mCumulativeWeights.clear();
        mCumulativeWeights.reserve(mRandomEntries.size());
        for (const auto& entry : mRandomEntries) {
            mTotalWeight += entry.weight;
            mCumulativeWeights.push_back(mTotalWeight);
        }
    }

    std::vector<RollTableEntry<T>> mGuaranteedEntries;
    std::vector<RollTableEntry<T>> mRandomEntries;
    std::vector<f32> mCumulativeWeights; // For binary search
    f32 mEmptyWeight = 0.0f;
    f32 mTotalWeight = 0.0f;
    i32 mMaximumGuaranteedEntries = 0;
};

template <typename T>
bool RollTable<T>::ymlRead(c4::yml::ConstNodeRef const& n) {
    mRandomEntries.clear();
    mCumulativeWeights.clear();
    mTotalWeight = 0.0f;

    if (!n.is_map()) {
        return true;
    }

    auto readList = [](const c4::yml::ConstNodeRef& n, std::vector<RollTableEntry<T>>& entries) {
        for (const auto& entry : n) {
            RollTableEntry<T> e;
            if (entry.has_child("sub_table")) {
                e.subTable = std::make_unique<RollTable<T>>();
                e.subTable->ymlRead(entry["sub_table"]);
            }
            else {
                if (entry.has_child("val")) {
                    entry["val"] >> e.value;
                }
                if (entry.has_child("count")) {
                    entry["count"] >> e.quantityRange;
                }
                if (entry.has_child("weight")) {
                    entry["weight"] >> e.weight;
                }
            }
            entries.push_back(std::move(e));
        }
    };

    if (n.has_child("guar")) {
        readList(n["guar"], mGuaranteedEntries);
    }

    if (n.has_child("rand")) {
        readList(n["rand"], mRandomEntries);
    }

    if (n.has_child("e_weight")) {
        n["e_weight"] >> mEmptyWeight;
    }

    return true;
}

template <typename T>
void RollTable<T>::ymlWrite(c4::yml::NodeRef& n) const {
    n |= ryml::MAP;

    // Helper
    auto writeList = [this](c4::yml::NodeRef& n, const char* name, const std::vector<RollTableEntry<T>>& entries) {
        c4::yml::NodeRef r = n.append_child();
        r << ryml::key(name);
        r |= ryml::SEQ;
        for (const auto& entry : entries) {
            c4::yml::NodeRef c = r.append_child();
            c |= ryml::MAP;
            if (entry.subTable) {
                c4::yml::NodeRef st = c.append_child();
                st << ryml::key("sub_table");
                entry.subTable->ymlWrite(st);
            }
            else {
                c["val"] << entry.value;
                // Random only
                if (&entries == &mRandomEntries) {
                    c["weight"] << entry.weight;
                }
                c["count"] << entry.quantityRange;
            }
        }
    };

    writeList(n, "rand", mRandomEntries);
    writeList(n, "guar", mGuaranteedEntries);

    if (mEmptyWeight > 0.0f) {
        n["e_weight"] << mEmptyWeight;
    }
   
}

template <typename T>
i32 RollTable<T>::rollN(std::span<Result> outputBuffer, i32 n) const
{
    assert(mRandomEntries.size());
    assert(outputBuffer.size() >= getRequiredBufferSizeForRollCount(n));

    i32 count = 0;

    // Helper
    auto rollEntry = [&](const RollTableEntry<T>& entry) {
        if (entry.subTable) {
            count += entry.subTable->rollN(outputBuffer.subspan(count, 1), 1);
        }
        else {
            Result& result = outputBuffer[count++];
            result.value = entry.value;
            if (entry.quantityRange.x == entry.quantityRange.y) {
                result.quantity = entry.quantityRange.x;
            }
            else {
                result.quantity = sThreadLocalRandomGenerator.getRandomIntInRange(entry.quantityRange.x, entry.quantityRange.y);
            }
        }
    };

    // First add guaranteed entries
    for (const auto& entry : mGuaranteedEntries) {
        rollEntry(entry);
    }

    // Now roll random entries
    for (ui32 i = 0; i < n; i++) {
        const f32 roll = sThreadLocalRandomGenerator.getRandomFloatUnsigned() * mTotalWeight;
        auto it = std::upper_bound(mCumulativeWeights.begin(), mCumulativeWeights.end(), roll);
        if (it == mCumulativeWeights.end()) [[unlikely]] {
            // Fail case
            continue;
        }
        const int index = std::distance(mCumulativeWeights.begin(), it);
        const auto& entry = mRandomEntries[index];

        rollEntry(entry);
    }
    return count;
}

// Utility function to calculate and display the probability table, handling recursive subtables
template <typename T>
void RollTable<T>::displayProbabilityTable(float parentProbability, int depth) const {
    // Calculate probabilities and quantities for random entries
    i32 i = 0;
    for (const auto& entry : mRandomEntries) {
        float probability = parentProbability * (entry.weight / mTotalWeight);
        float avgQuantity = (entry.quantityRange.x + entry.quantityRange.y) / 2.0f;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Indent(depth * 20);  // Indent based on depth
        //if (!ImguiUtil::displayValue(entry.value)) {
            ImGui::Text("%d", i);
        //}
        ImGui::Unindent();

        ImGui::TableNextColumn();
        ImGui::Text("%.2f%%", probability);

        ImGui::TableNextColumn();
        ImGui::Text("%.2f", avgQuantity);

        // Handle recursive subtable
        if (entry.subTable) {
            entry.subTable->displayProbabilityTable(probability, depth + 1);
        }
        ++i;
    }

    // Display probability for the empty item
    if (mEmptyWeight > 0.0f) {
        float emptyProbability = parentProbability * (mEmptyWeight / mTotalWeight);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Indent(depth * 20);  // Indent based on depth
        ImGui::Text("Empty");
        ImGui::Unindent();

        ImGui::TableNextColumn();
        ImGui::Text("%.2f%%", emptyProbability);

        ImGui::TableNextColumn();
        ImGui::Text("-");
    }
}

template <typename T>
bool RollTable<T>::updateAndRenderImgui(const char* label)
{
    bool changed = false;

    if (!ImGui::TreeNode(label)) {
        return false;
    }

    changed |= ImGui::InputFloat("Empty Weight", &mEmptyWeight);

    // Helper
    auto updateAndRenderEntry = [this](RollTableEntry<T>& o) -> bool {
        bool changed = false;
        if (o.subTable) {
            if (ImGui::Button("Remove Subtable")) {
                o.subTable.reset();
                changed = true;
            }
            else {
                ImGui::Indent();
                changed |= o.subTable->updateAndRenderImgui("Subtable");
                ImGui::Unindent();
            }
        }
        else {
            if (ImGui::Button("To Subtable")) {
                o.subTable = std::make_unique<RollTable<T>>();
                changed = true;
            }
            else {
                changed |= ImguiUtil::updateAndRenderImgui("Value", o.value);
                changed |= ImGui::InputInt2("Quantity Range", &o.quantityRange.x);
            }
        }
        return changed;
    };

    // Guaranteed entries
    changed |= ImguiUtil::ObjectVector<RollTableEntry<T>>("Guaranteed Entries", mGuaranteedEntries,
        [&updateAndRenderEntry](RollTableEntry<T>& o, ui32 index) {
        return updateAndRenderEntry(o);
    });

    // Random entries
    changed = ImguiUtil::ObjectVector<RollTableEntry<T>>("Random Entries", mRandomEntries,
        [this, &updateAndRenderEntry](RollTableEntry<T>& o, ui32 index) {
        bool changed = false;
        changed |= ImGui::InputFloat("Weight", &o.weight);
        changed |= updateAndRenderEntry(o);
        return changed;
    });

    // Display the probability table
    if (ImGui::TreeNode("View Probability Table")) {
        if (ImGui::BeginTable("Probability Table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Item");
            ImGui::TableSetupColumn("True Probability (%)");
            ImGui::TableSetupColumn("Avg Quantity on Success");
            ImGui::TableHeadersRow();
            displayProbabilityTable();
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }

    if (changed) {
        updateMaxGuaranteedEntries();
        updateWeight();
    }

    ImGui::TreePop();

    return changed;
}
