#pragma once

#include <boost/container/flat_map.hpp>

namespace bitsery
{
    namespace ext
    {
        // Extension for writing a vector of POD structs that have trivial serialize methods.
        // This greatly improves performance when we do not need to swap bytes, but will fall back
        // to the serialize method of the POD struct if we do need to swap.
        class PodStructVector
        {
        public:
            template <typename Ser,
                typename T,
                typename Fnc>
            void serialize(Ser& s, const std::vector<T>& vec, Fnc&&) const
            {
                static_assert(std::is_standard_layout<T>::value, "T must be a POD type");

                auto& writer = s.adapter();
                writer.template writeBytes<4>(static_cast<uint32_t>(vec.size()));
                if constexpr (bitsery::details::ShouldSwap<typename BOutputAdapter::TConfig>{}) {
                    s.container(vec, vec.size());
                }
                else {
                    // Treat as an array of bytes
                    writer.template writeBuffer<1>(reinterpret_cast<const uint8_t*>(vec.data()), vec.size() * sizeof(T));
                }
            }

            template <typename Des,
                typename T,
                typename Fnc>
            void deserialize(Des& s, std::vector<T>& vec, Fnc&&) const
            {
                static_assert(std::is_standard_layout<T>::value, "T must be a POD type");

                auto& reader = s.adapter();
                uint32_t count = 0u;
                reader.template readBytes<4>(count);
                vec.resize(count);

                if constexpr (bitsery::details::ShouldSwap<typename BInputAdapter::TConfig>{}) {
                    s.container(vec, count);
                }
                else {
                    // Treat as an array of bytes
                    reader.template readBuffer<1>(reinterpret_cast<uint8_t*>(vec.data()), count * sizeof(T));
                }
            }
        };
    } // namespace ext

    namespace traits
    {
        template <typename T>
        struct ExtensionTraits<ext::PodStructVector, T>
        {
            using TValue = void;
            static constexpr bool SupportValueOverload = false;
            static constexpr bool SupportObjectOverload = true;
            static constexpr bool SupportLambdaOverload = false;
        };
    } // namespace traits


    namespace ext
    {
        // Extension for writing a span of POD structs that have trivial serialize methods.
        // This greatly improves performance when we do not need to swap bytes, but will fall back
        // to the serialize method of the POD struct if we do need to swap.
        class PodStructSpan
        {
        public:
            template <typename Ser,
                typename T,
                typename Fnc>
            void serialize(Ser& s, const std::span<T>& vec, Fnc&&) const
            {
               // static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value, "T must be a POD type");

                auto& writer = s.adapter();
                writer.template writeBytes<4>(static_cast<uint32_t>(vec.size()));
                if constexpr (bitsery::details::ShouldSwap<typename BOutputAdapter::TConfig>{}) {
                    //s.container(vec, vec.size());
                    assert(false);
                }
                else {
                    // Treat as an array of bytes
                    writer.template writeBuffer<1>(reinterpret_cast<const uint8_t*>(vec.data()), vec.size() * sizeof(T));
                }
            }

            template <typename Des,
                typename T,
                typename Fnc>
            void deserialize(Des& s, std::span<T>& vec, Fnc&&) const
            {
               // static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value, "T must be a POD type");

                auto& reader = s.adapter();
                uint32_t count = 0u;
                reader.template readBytes<4>(count);
                // Right now spans can only be statically sized
                assert(vec.size() == count);

                if constexpr (bitsery::details::ShouldSwap<typename BInputAdapter::TConfig>{}) {
                    //s.container(vec, count);
                    assert(false);
                }
                else {
                    // Treat as an array of bytes
                    reader.template readBuffer<1>(reinterpret_cast<uint8_t*>(vec.data()), count * sizeof(T));
                }
            }
        };
    } // namespace ext

    namespace traits
    {
        template <typename T>
        struct ExtensionTraits<ext::PodStructSpan, T>
        {
            using TValue = void;
            static constexpr bool SupportValueOverload = false;
            static constexpr bool SupportObjectOverload = true;
            static constexpr bool SupportLambdaOverload = false;
        };
    } // namespace traits
} // namespace bitsery



namespace bitsery {
    namespace ext {

        class BoostFlatMap
        {
        public:
            constexpr explicit BoostFlatMap(size_t maxSize)
                : _maxSize{ maxSize }
            {
            }

            template<typename Ser, typename T, typename Fnc>
            void serialize(Ser& ser, const T& obj, Fnc&& fnc) const
            {
                using TKey = typename T::key_type;
                using TValue = typename T::mapped_type;
                auto size = obj.size();
                assert(size <= _maxSize);
                details::writeSize(ser.adapter(), size);

                for (auto& v : obj)
                    fnc(ser, const_cast<TKey&>(v.first), const_cast<TValue&>(v.second));
            }

            template<typename Des, typename T, typename Fnc>
            void deserialize(Des& des, T& obj, Fnc&& fnc) const
            {
                using TKey = typename T::key_type;
                using TValue = typename T::mapped_type;

                size_t size{};
                details::readSize(
                    des.adapter(),
                    size,
                    _maxSize,
                    std::integral_constant<bool, Des::TConfig::CheckDataErrors>{});
                obj.clear();
                reserve(obj, size);

                auto hint = obj.begin();
                for (auto i = 0u; i < size; ++i) {
                    auto key = bitsery::Access::create<TKey>();
                    auto value = bitsery::Access::create<TValue>();
                    fnc(des, key, value);
                    hint = obj.emplace_hint(hint, std::move(key), std::move(value));
                }
            }

        private:
            template<typename Key,
                typename T,
                typename Hash,
                typename KeyEqual,
                typename Allocator>
            void reserve(std::unordered_map<Key, T, Hash, KeyEqual, Allocator>& obj,
                size_t size) const
            {
                obj.reserve(size);
            }
            template<typename Key,
                typename T,
                typename Hash,
                typename KeyEqual,
                typename Allocator>
            void reserve(std::unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& obj,
                size_t size) const
            {
                obj.reserve(size);
            }
            template<typename T>
            void reserve(T&, size_t) const
            {
                // for ordered container do nothing
            }
            size_t _maxSize;
        };
    }

    namespace traits {
        template<typename T>
        struct ExtensionTraits<ext::BoostFlatMap, T>
        {
            using TValue = void;
            static constexpr bool SupportValueOverload = false;
            static constexpr bool SupportObjectOverload = false;
            static constexpr bool SupportLambdaOverload = true;
        };
    }

}