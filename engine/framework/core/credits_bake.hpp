#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace framework::core {

struct CreditSrc {
    std::string pack;
    std::string author;
    std::string license;
    std::string url;
    std::string attribution;
};

bool parse_credits(std::string_view text, const std::string& file, std::vector<CreditSrc>& out,
                   std::string& error);
bool bake_credits(std::span<const CreditSrc> credits, std::vector<uint8_t>& out, std::string& error);

} // namespace framework::core
