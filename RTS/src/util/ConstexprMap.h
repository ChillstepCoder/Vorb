#pragma once

// https://xuhuisun.com/post/c++-weekly-2-constexpr-map/
template <typename Key, typename Value, std::size_t Size>
struct ConstexprMap {

    constexpr ConstexprMap(std::array<std::pair<Key, Value>, Size> data) : data(data) {};

    std::array<std::pair<Key, Value>, Size> data;

    [[nodiscard]] constexpr Value operator[](const Key& key) const noexcept {
        const auto itr = std::find_if(begin(data), end(data),
                [&key](const auto& v) { return v.first == key; });
        if (itr != end(data)) {
            return itr->second;
        }
        else {
            assert(false && "Constexpr map is missing a key");
            return Value();
        }
    }

    [[nodiscard]] constexpr Key getKeyForValue(const Value& value) const noexcept {
        const auto itr = std::find_if(begin(data), end(data),
            [&value](const auto& v) { return v.second == value; });
        if (itr != end(data)) {
            return itr->first;
        }
        else {
            assert(false && "Wrong key name found in file");
            return Key();
        }
    }
};
// Template deduction guide
template <typename Key, typename Value, std::size_t Size>
ConstexprMap(const std::array<std::pair<Key, Value>, Size>&) -> ConstexprMap<Key, Value, Size>;
