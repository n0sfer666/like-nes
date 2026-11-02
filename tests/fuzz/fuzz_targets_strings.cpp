#include "fuzz_target.hpp"
#include "strings.hpp"
#include "strings_fixture.hpp"

namespace fuzz {
namespace {
std::vector<uint8_t> seed_strings() {
    std::vector<uint8_t> bytes;
    framework::ui::StringsStats stats; std::string error;
    framework::ui::bake_strings(strings_fixture::rows(), strings_fixture::font(), false, bytes, stats, error);
    return bytes;
}
bool read_strings(const uint8_t* data, size_t size) {
    framework::ui::Strings table;
    if (!table.open(data, size)) return false;
    consume_all(table.count(), table.fallbacks()); consume_str(table.font_name());
    for (uint32_t i = 0; i < table.count(); ++i) {
        consume_str(table.key(i));
        for (const auto lang : {framework::ui::Language::En, framework::ui::Language::Ru}) {
            const auto text = table.find(table.key(i), lang);
            consume(text.size());
            for (const auto byte : text) consume(static_cast<uint8_t>(byte));
        }
    }
    return true;
}
const Target TARGETS[]{{"strings", seed_strings, read_strings}};
}
const Target* strings_targets(size_t* count) { *count = sizeof TARGETS / sizeof TARGETS[0]; return TARGETS; }
}
