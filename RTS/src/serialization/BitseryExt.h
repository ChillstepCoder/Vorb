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