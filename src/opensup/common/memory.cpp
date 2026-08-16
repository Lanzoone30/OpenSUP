// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder

#include "opensup/pch.h"
#include "opensup/common/memory.h"

#include <cstdlib>
#include <cstring>

namespace opensup {
namespace common {

memory_cptr
memory_c::alloc(size_t size)
{
    memory_cptr mem(new memory_c);
    if (size > 0) {
        auto* ptr = static_cast<uint8_t*>(std::malloc(size));
        if (!ptr) throw std::bad_alloc();
        mem->m_data.reset(ptr);
        mem->m_size = size;
    }
    return mem;
}

memory_cptr
memory_c::clone(const uint8_t* data, size_t size)
{
    auto mem = alloc(size);
    if (size > 0) {
        std::memcpy(mem->m_data.get(), data, size);
    }
    return mem;
}

memory_cptr
memory_c::clone(const std::vector<uint8_t>& data)
{
    return clone(data.data(), data.size());
}

memory_c::memory_c(memory_c&& other) noexcept
    : m_data(std::move(other.m_data))
    , m_size(other.m_size)
{
    other.m_size = 0;
}

memory_c&
memory_c::operator=(memory_c&& other) noexcept
{
    if (this != &other) {
        m_data = std::move(other.m_data);
        m_size = other.m_size;
        other.m_size = 0;
    }
    return *this;
}

std::vector<uint8_t>
memory_c::to_vector() const
{
    if (m_size == 0) return {};
    return std::vector<uint8_t>(m_data.get(), m_data.get() + m_size);
}

} // namespace common
} // namespace opensup
