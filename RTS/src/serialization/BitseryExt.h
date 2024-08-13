#pragma once


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
            void deserialize(Des& s, std::span<T>& vec, Fnc&&) const
            {
               // static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value, "T must be a POD type");

                auto& reader = s.adapter();
                uint32_t count = 0u;
                reader.template readBytes<4>(count);
                // Right now spans can only be statically sized
                assert(vec.size() == count);

                if constexpr (bitsery::details::ShouldSwap<typename BInputAdapter::TConfig>{}) {
                    s.container(vec, count);
                }
                else {
                    // Treat as an array of bytes
                    reader.template readBuffer<1>(reinterpret_cast<uint8_t*>(vec.data()), count * sizeof(T));
                }
            }
        };

        // Extension for writing a span of POD structs that have trivial serialize methods.
        // This greatly improves performance when we do not need to swap bytes, but will fall back
        // to the serialize method of the POD struct if we do need to swap.
        class PodStructRawPointerArray
        {
        public:
            PodStructRawPointerArray(ui32& outSize) : mOutSize(&outSize) {};
            PodStructRawPointerArray(const ui32& inSize) : mInSize(&inSize) {};

            template <typename Ser,
                typename T,
                typename Fnc>
            void serialize(Ser& s, T& ptr, Fnc&&) const
            {
                static_assert(std::is_pointer<T>::value);
                assert(mInSize);
                // static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value, "T must be a POD type");

                auto& writer = s.adapter();
                s.value4b(*mInSize);
                if constexpr (bitsery::details::ShouldSwap<typename BOutputAdapter::TConfig>{}) {
                    for (uint32_t i = 0; i < (*mInSize); ++i) {
                        s.writeBytes(ptr[i]);
                    }
                }
                else {
                    // Treat as an array of bytes
                    writer.template writeBuffer<1>(reinterpret_cast<const uint8_t*>(ptr), (*mInSize) * sizeof(*ptr));
                }
            }

            template <typename Des,
                typename T,
                typename Fnc>
            void deserialize(Des& s, T& ptr, Fnc&&) const
            {
                static_assert(std::is_pointer<T>::value);
                assert(mOutSize);
                using ElementType = std::remove_pointer_t<T>;

                auto& reader = s.adapter();
                s.value4b(*mOutSize);
                ptr = new ElementType[(*mOutSize)];

                if constexpr (bitsery::details::ShouldSwap<typename BInputAdapter::TConfig>{}) {
                    for (uint32_t i = 0; i < (*mOutSize); ++i) {
                        s.readBytes(ptr[i]);
                    }
                }
                else {
                    // Treat as an array of bytes
                    reader.template readBuffer<1>(reinterpret_cast<uint8_t*>(ptr), (*mOutSize) * sizeof(*ptr));
                }
            }
        private:
            ui32* mOutSize = nullptr;
            const ui32* mInSize = nullptr;
        };

        // Extension for writing a span of POD structs that have trivial serialize methods.
        // This greatly improves performance when we do not need to swap bytes, but will fall back
        // to the serialize method of the POD struct if we do need to swap.
        class PodStructUniquePointerArray
        {
        public:
            PodStructUniquePointerArray(ui32& outSize) : mOutSize(&outSize) {};
            PodStructUniquePointerArray(const ui32& inSize) : mInSize(&inSize) {};

            template <typename Ser,
                typename T,
                typename Fnc>
            void serialize(Ser& s, T& ptr, Fnc&&) const
            {
                assert(mInSize);

                auto& writer = s.adapter();
                s.value4b(*mInSize);
                if constexpr (bitsery::details::ShouldSwap<typename BOutputAdapter::TConfig>{}) {
                    for (uint32_t i = 0; i < (*mInSize); ++i) {
                        s.writeBytes(ptr[i]);
                    }
                }
                else {
                    // Treat as an array of bytes
                    writer.template writeBuffer<1>(reinterpret_cast<const uint8_t*>(ptr.get()), (*mInSize) * sizeof(T::element_type));
                }
            }

            template <typename Des,
                typename T,
                typename Fnc>
            void deserialize(Des& s, T& ptr, Fnc&&) const
            {
                assert(mOutSize);

                using ElementType = typename T::element_type;

                auto& reader = s.adapter();
                s.value4b(*mOutSize);

                // Create a unique_ptr with a custom deleter that deletes an array
                ptr = T(new ElementType[(*mOutSize)]);

                if constexpr (bitsery::details::ShouldSwap<typename BInputAdapter::TConfig>{}) {
                    for (uint32_t i = 0; i < (*mOutSize); ++i) {
                        static_assert(std::is_trivially_copyable_v<ElementType>);
                        if constexpr (std::is_trivially_copyable_v<ElementType>) {
                            s.readBytes(ptr.get()[i]);
                        }
                        else {
                            //customDeserialize(s, ptr.get()[i]);
                            assert(false);
                        }
                    }
                }
                else {
                    static_assert(std::is_trivially_copyable_v<ElementType>);
                    if constexpr (std::is_trivially_copyable_v<ElementType>) {
                        // Treat as an array of bytes for trivially copyable types
                        reader.template readBuffer<1>(reinterpret_cast<uint8_t*>(ptr.get()), (*mOutSize) * sizeof(ElementType));
                    }
                    else {
                        // Use custom deserialization for non-trivially copyable types
                        for (uint32_t i = 0; i < (*mOutSize); ++i) {
                            //customDeserialize(s, ptr.get()[i]);
                            assert(false);
                        }
                    }
                }
            }
        private:
            ui32* mOutSize;
            const ui32* mInSize;
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

        template <typename T>
        struct ExtensionTraits<ext::PodStructRawPointerArray, T>
        {
            using TValue = void;
            static constexpr bool SupportValueOverload = false;
            static constexpr bool SupportObjectOverload = true;
            static constexpr bool SupportLambdaOverload = false;
        };

        template <typename T>
        struct ExtensionTraits<ext::PodStructUniquePointerArray, T>
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
                obj.reserve(size);

                auto hint = obj.begin();
                for (auto i = 0u; i < size; ++i) {
                    auto key = bitsery::Access::create<TKey>();
                    auto value = bitsery::Access::create<TValue>();
                    fnc(des, key, value);
                    hint = obj.emplace_hint(hint, std::move(key), std::move(value));
                }
            }

        private:
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

namespace bitsery
{
    namespace ext
    {
        // Extension for writing a single POD struct in straight binary.
        class PodStruct
        {
        public:
            template <typename Ser,
                typename T,
                typename Fnc>
            void serialize(Ser& s, const T& obj, Fnc&&) const
            {
                static_assert(std::is_standard_layout<T>::value, "T must be a POD type");

                auto& writer = s.adapter();
                writer.template writeBuffer<1>(reinterpret_cast<const uint8_t*>(&obj), sizeof(T));
            }

            template <typename Des,
                typename T,
                typename Fnc>
            void deserialize(Des& s, T& obj, Fnc&&) const
            {
                static_assert(std::is_standard_layout<T>::value, "T must be a POD type");

                auto& reader = s.adapter();
                reader.template readBuffer<1>(reinterpret_cast<uint8_t*>(&obj), sizeof(T));
            }
        };
    } // namespace ext

    namespace traits
    {
        template <typename T>
        struct ExtensionTraits<ext::PodStruct, T>
        {
            using TValue = void;
            static constexpr bool SupportValueOverload = false;
            static constexpr bool SupportObjectOverload = true;
            static constexpr bool SupportLambdaOverload = false;
        };
    } // namespace traits
} // namespace bitsery
