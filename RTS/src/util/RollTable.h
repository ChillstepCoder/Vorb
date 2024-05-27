#pragma once

template <typename T>
class RollTable;

template <typename T>
struct RollTableEntry {
    T item;
    f32 probability = 0.0f;
    i32v2 quanityRange = i32v2(0);
    std::unique_ptr<RollTable<T>> subTable = nullptr;
};

// TODO: New file
template <typename T>
class RollTable {
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

private:
    std::vector<RollTableEntry<T>> mEntries;
    std::vector<f32> mCumulativeProbabilities; // For binary search
    f32 mTotalProbability = 0.0f;
};