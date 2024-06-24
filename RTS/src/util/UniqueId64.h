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

inline size_t hash_value(const UniqueId64& uuid) {
    return boost::hash<ui64>()((ui64)uuid);
};

