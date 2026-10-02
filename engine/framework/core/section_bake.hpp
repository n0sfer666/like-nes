#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace framework::core {

class SectionBuilder {
public:
    explicit SectionBuilder(std::size_t rows_bytes);
    uint32_t block(const void* data, std::size_t bytes, std::size_t align);
    uint32_t text(const std::string& s);
    bool finish(const uint8_t (&magic)[4], uint32_t version, uint32_t count, const void* rows,
                std::vector<uint8_t>& out, std::string& error);

private:
    std::size_t rows_bytes_;
    std::vector<uint8_t> bytes_;
    std::vector<char> blob_{'\0'};
    std::map<std::string, uint32_t> seen_{{std::string(), 0}};
};

} // namespace framework::core
