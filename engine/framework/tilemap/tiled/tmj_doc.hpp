#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "byte_arena.hpp"
#include "fixed.hpp"
#include "json.hpp"

namespace framework::tiled {

enum class Need : uint8_t { Optional, Required };

struct Doc {
    std::string file;
    std::span<const std::byte> in;
    asset::ByteArena arena;
    std::vector<json::Node> nodes;

    bool load(std::string& error);
    const json::Node& root() const { return nodes[0]; }
    std::span<const json::Node> items(const json::Node& array) const;
    bool fail(const json::Node& at, const std::string& message, std::string& error) const;
    const json::Node* field(const json::Node& obj, std::string_view key) const;
    bool expect(const json::Node& obj, std::string_view key, json::Kind kind, Need need,
                const json::Node*& out, std::string& error) const;
    bool get(const json::Node& obj, std::string_view key, int64_t& out, std::string& error,
             Need need = Need::Optional) const;
    bool get(const json::Node& obj, std::string_view key, fix32& out, std::string& error,
             Need need = Need::Optional) const;
    bool get(const json::Node& obj, std::string_view key, bool& out, std::string& error,
             Need need = Need::Optional) const;
    bool get(const json::Node& obj, std::string_view key, std::string_view& out, std::string& error,
             Need need = Need::Optional) const;
    bool prop(const json::Node& obj, std::string_view name, std::string_view type, const json::Node*& value,
              std::string& error) const;
};

} // namespace framework::tiled
