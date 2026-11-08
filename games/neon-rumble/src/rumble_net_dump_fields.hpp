#pragma once
#include "rumble_net_sim.hpp"

#include <charconv>
#include <type_traits>

namespace rumble {

class DumpFields {
public:
    explicit DumpFields(std::string& text, std::string prefix = {}) : text_(text), prefix_(std::move(prefix)) {}

    DumpFields child(std::string_view label) const { return DumpFields(text_, prefix_ + std::string(label) + "."); }
    DumpFields at(size_t index) const { return child(std::to_string(index)); }

    template<class T>
    void put(std::string_view name, T value) const {
        if constexpr (std::is_enum_v<T>) {
            put(name, static_cast<std::underlying_type_t<T>>(value));
        } else if constexpr (std::is_same_v<T, fix32>) {
            put(name, value.raw);
        } else if constexpr (std::is_same_v<T, bool>) {
            put(name, value ? 1 : 0);
        } else {
            char digits[32];
            const auto end = std::to_chars(digits, digits + sizeof(digits), value);
            text_ += prefix_;
            text_ += name;
            text_.push_back('=');
            text_.append(digits, end.ptr);
            text_.push_back('\n');
        }
    }

private:
    std::string& text_;
    std::string prefix_;
};

void dump_pool(const br::BodyPool& pool, DumpFields out);
void dump_world(const NetworkSnapshot& snapshot, DumpFields out);

} // namespace rumble
