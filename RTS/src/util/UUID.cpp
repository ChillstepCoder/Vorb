#include "stdafx.h"
#include "UUID.h"

#include <random>

static std::random_device sRandomDevice;
static std::mt19937_64 eng(sRandomDevice());
static std::uniform_int_distribution<ui64> sUniformDistribution;

static std::mt19937 eng32(sRandomDevice());
static std::uniform_int_distribution<uint32_t> sUniformDistribution32;

UUID::UUID() : m_UUID(sUniformDistribution(eng)) {
}

UUID::UUID(ui64 uuid) : m_UUID(uuid) {
}

UUID::UUID(const UUID& other) : m_UUID(other.m_UUID) {
}


