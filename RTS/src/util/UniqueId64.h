#pragma once
#include <xhash>

    // From Hazel
    // "UUID" (universally unique identifier) or GUID is (usually) a 128-bit integer
    // used to "uniquely" identify information. In Hazel, even though we use the term
    // GUID and UUID, at the moment we're simply using a randomly generated 64-bit
    // integer, as the possibility of a clash is low enough for now.
    // This may change in the future.
    class UniqueId64 {
    public:
        UniqueId64();
        UniqueId64(ui64 uuid);
        UniqueId64(const UniqueId64& other);

        static UniqueId64 Generate();

        operator const ui64() const { return m_UUID; }

        //bool operator==(const UniqueId64& other) const { return m_UUID == other.m_UUID; }
        //bool operator<(const UniqueId64& other) const { return m_UUID < other.m_UUID; }

        bool isValid() const { return m_UUID != 0; }
    private:
        ui64 m_UUID = 0;
    };

namespace std {

    template <>
    struct hash<UniqueId64>
    {
        std::size_t operator()(const UniqueId64& uuid) const
        {
            // uuid is already a randomly generated number, and is suitable as a hash key as-is.
            // this may change in future, in which case return hash<ui64>{}(uuid); might be more appropriate
            return uuid;
        }
    };
}
