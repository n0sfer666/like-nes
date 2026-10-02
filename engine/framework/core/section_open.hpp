#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "section_format.hpp"

namespace framework::core {

struct SectionView {
    const uint8_t* base = nullptr;
    const SectionHeader* header = nullptr;
    uint32_t rows_end = 0;
    const char* strings = nullptr;
    uint32_t strings_size = 0;

    bool block(uint32_t offset, uint64_t bytes, std::size_t align) const;
    bool text(uint32_t offset) const { return offset < strings_size; }

    template <class T>
    std::span<const T> at(uint32_t offset, uint32_t count) const {
        // Zero-parse: границы и выравнивание блока под T проверены `block` или `open_section`
        // до первого обращения (ADR 0003).
        return {reinterpret_cast<const T*>(base + offset), count};
    }

    template <class T>
    bool take(uint32_t offset, uint32_t count, std::span<const T>& out) const {
        if (!block(offset, uint64_t{count} * sizeof(T), alignof(T))) return false;
        out = at<T>(offset, count);
        return true;
    }
};

bool open_section(const void* data, std::size_t size, const uint8_t (&magic)[4], uint32_t version,
                  std::size_t row_size, std::size_t align, SectionView& out);

} // namespace framework::core
