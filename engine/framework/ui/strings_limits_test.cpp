#include "strings.hpp"
#include "strings_bake.hpp"
#include "strings_fixture.hpp"

#include <cstdio>
#include <cstring>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"

int main() {
    using namespace framework::ui;
    int fails = 0;
    auto check = [&](bool good, const char* why) { if (!good) { ++fails; std::printf("strings-limits: FAIL %s\n", why); } };
    auto font = strings_fixture::font();
    std::vector<StringSrc> rows;
    for (uint32_t i = 0; i < MAX_STRINGS; ++i) rows.push_back({"key." + std::to_string(i), "A", ""});
    std::vector<uint8_t> bytes; StringsStats stats; std::string error;
    check(bake_strings(rows, font, false, bytes, stats, error) && stats.keys == MAX_STRINGS && stats.fallbacks == MAX_STRINGS, "maximum rows");
    Strings table;
    check(table.open(bytes.data(), bytes.size()) && table.count() == MAX_STRINGS, "maximum rows read");
    rows.push_back({"extra", "A", "A"});
    const auto before = bytes;
    check(!bake_strings(rows, font, false, bytes, stats, error) && bytes == before, "max+1 rows refused before indexing");
    const StringSrc long_key{std::string(MAX_STRING_KEY, 'k'), "A", "A"};
    std::vector<uint8_t> key_bytes;
    check(bake_strings({&long_key, 1}, font, false, key_bytes, stats, error) && table.open(key_bytes.data(), key_bytes.size()) &&
        table.find(long_key.key) == "A", "maximum key length");

    // One 1-MiB phrase shared by all 4096 rows: validation must not scan it 8192 times.
    framework::core::SectionHeader header;
    std::memcpy(&header, bytes.data(), sizeof header);
    const uint32_t value = static_cast<uint32_t>(bytes.size()) - header.strings_offset;
    constexpr uint32_t length = 1u << 20;
    bytes.insert(bytes.end(), length, 'A'); bytes.push_back(0);
    header.total_size = static_cast<uint32_t>(bytes.size());
    std::memcpy(bytes.data(), &header, sizeof header);
    // Zero-parse fixture: vector allocation and the pinned row offset are aligned for StringRow.
    auto* wire_rows = reinterpret_cast<StringRow*>(bytes.data() + header.rows_offset);
    for (uint32_t i = 0; i < MAX_STRINGS; ++i) {
        wire_rows[i].en = wire_rows[i].ru = value;
        wire_rows[i].en_size = wire_rows[i].ru_size = length;
    }
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    const bool control = framework::probe::control::plain_allocation() && framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(control && framework::probe::allocs >= 2, "allocation counter positive control");
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    const bool opened = table.open(bytes.data(), bytes.size());
    const auto text = table.find("key.4095", Language::Ru);
    framework::probe::in_hot = false;
    check(opened && text.size() == length && text.front() == 'A' && text.back() == 'A' && framework::probe::allocs == 0,
          "shared large value validated/read with no heap allocation");
    check(table.find("key.4095", static_cast<Language>(255)).empty() && std::string_view(table.key(MAX_STRINGS)).empty(), "invalid language/index safe");
    bytes[header.strings_offset + value + 17] = 0;
    check(!table.open(bytes.data(), bytes.size()) && table.count() == 0, "embedded value NUL cannot hide inside declared length");
    {
        std::vector<uint8_t> huge(MAX_STRINGS_BYTES + 1);
        check(!table.open(huge.data(), huge.size()), "oversized table refused before scan");
    }
    {
        std::string huge(MAX_STRINGS_BYTES + 1, 'A');
        std::vector<StringSrc> parsed;
        check(!parse_strings_tsv(huge, "huge.tsv", parsed, error) && parsed.empty(), "oversized TSV refused before rows");
    }
    check(!bake_strings({}, font, false, bytes, stats, error), "empty source refused");
    {
        const std::string shared(1u << 20, 'A');
        std::vector<StringSrc> repeated;
        for (int i = 0; i < 17; ++i) repeated.push_back({"shared." + std::to_string(i), shared, ""});
        check(bake_strings(repeated, font, false, bytes, stats, error) && bytes.size() < (2u << 20) &&
              table.open(bytes.data(), bytes.size()) && table.find("shared.16", Language::Ru).size() == shared.size(),
              "encoded budget accounts for dedup, not repeated input bytes");
    }
    std::puts(fails ? "strings-limits: FAIL" : "strings-limits: PASS caps, large shared values, no allocation and malformed lengths");
    return fails ? 1 : 0;
}
