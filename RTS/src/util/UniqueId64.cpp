#include "stdafx.h"
#include "UniqueId64.h"

#include <random>

static std::random_device sRandomDevice;
static std::mt19937_64 eng(sRandomDevice());
static std::uniform_int_distribution<ui64> sUniformDistribution;

static std::mt19937 eng32(sRandomDevice());
static std::uniform_int_distribution<uint32_t> sUniformDistribution32;

UniqueId64::UniqueId64() : m_UUID(0) {
}

UniqueId64::UniqueId64(ui64 uuid) : m_UUID(uuid) {
}

UniqueId64::UniqueId64(const UniqueId64& other) : m_UUID(other.m_UUID) {
}

UniqueId64 UniqueId64::Generate() {
    return sUniformDistribution(eng);
}

