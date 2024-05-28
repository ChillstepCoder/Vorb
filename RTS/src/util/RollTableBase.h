#pragma once

#include "math/Random.h"

template <typename T>
class RollTableBase;

template <typename T>
struct RollTableEntry {
    T item;
    f32 probability = 0.0f;
    i32v2 quanityRange = i32v2(0);
    std::unique_ptr<RollTableBase<T>> subTable = nullptr;
};

template <typename T>
class RollTableDef {

};

// TODO: New file
template <typename T>
class RollTableBase {
public:

    void setEntries(std::vector<RollTableEntry<T>>&& entries) {
        mEntries = std::move(entries);
        mTotalProbability = 0.0f;
        for (const auto& entry : mEntries) {
            mTotalProbability += entry.probability;
            mCumulativeProbabilities.push_back(mTotalProbability);
        }
    }

    // Returns number of drops returned in outputBuffer
    i32 rollN(std::span<std::pair<T, i32/*quantity*/>> outputBuffer, i32 n) {
        assert(mEntries.size());
        assert(outputBuffer.size() <= n);

        i32 count = 0;
        for (ui32 i = 0; i < n; i++) {
            f32 roll = sThreadLocalRandomGenerator.getRandomFloatUnsigned() * mTotalProbability;

            auto it = std::upper_bound(mCumulativeProbabilities.begin(), mCumulativeProbabilities.end(), roll);
            if (it == mCumulativeProbabilities.end()) [[unlikely]] {
                // This should be impossible I think, but just in case
                continue;
            }
            const int index = std::distance(mCumulativeProbabilities.begin(), it);
            const auto& entry = mEntries[index];

            if (entry.subTable) {
                count += entry.subTable->rollN(outputBuffer.subspan(count, 1), 1);
            }
            else {
                std::pair<T, i32/*quantity*/>& result = outputBuffer[count++];
                result.first = entry.item;
                if (entry.quanityRange.x == entry.quanityRange.y) {
                    result.second = entry.quanityRange.x;
                }
                else {
                    result.second = sThreadLocalRandomGenerator.getRandomIntInRange(entry.quanityRange.x, entry.quanityRange.y);
                }
            }
            break;
        }
        return count;
    }

    void ymlWrite(c4::yml::NodeRef& n) const {
        /*  n |= ryml::SEQ;
          for (const auto& entry : mEntries) {
              c4::ryml::NodeRef c = n.append_child();
              c |= ryml::MAP;
              if (entry.subTable) {
                  c4::ryml::NodeRef st = c.append_child();
                  st << ryml::key("sub_table");
                  entry.subTable->ymlWrite(st);
              }
              else {
                  c["item"] << entry.item;
                  c["probability"] << entry.probability;
                  c["quantityRange"] << quanityRange;
              }
          }*/
    }

    void ymlRead(c4::yml::ConstNodeRef const& n) {
        /* mEntries.clear();
         mCumulativeProbabilities.clear();
         mTotalProbability = 0.0f;

         for (const auto& c : n) {
             RollTableEntry<T> entry;
             if (c.has_child("sub_table")) {
                 entry.subTable = std::make_unique<RollTableBase<T>>();
                 entry.subTable->ymlRead(c["sub_table"]);
             }
             else {
                 c["item"] >> entry.item;
                 c["probability"] >> entry.probability;
                 c["quantityRange"] >> entry.quanityRange;
             }
             mEntries.push_back(std::move(entry));
         }*/
    }

protected:
    virtual void ymlWriteValue(c4::yml::NodeRef& s) const = 0;
    virtual void ymlReadValue(c4::yml::ConstNodeRef const& n) = 0;

    std::vector<RollTableEntry<T>> mEntries;
    std::vector<f32> mCumulativeProbabilities; // For binary search
    f32 mTotalProbability = 0.0f;
};
