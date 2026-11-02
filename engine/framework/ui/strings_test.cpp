#include "strings.hpp"
#include "strings_bake_internal.hpp"
#include "strings_fixture.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "framework_alloc_probe.hpp"
#include "framework_alloc_probe_control.hpp"
#include "hash.hpp"

namespace {
using namespace framework::ui;
int fails;
void check(bool good, const char* why) { if (!good) { ++fails; std::printf("strings: FAIL %s\n", why); } }
template<class T> T& at(std::vector<uint8_t>& bytes, size_t offset) { return *reinterpret_cast<T*>(bytes.data() + offset); }
}

int main() {
    auto font = strings_fixture::font();
    auto source = strings_fixture::rows();
    std::vector<uint8_t> bytes; StringsStats stats; std::string error;
    check(bake_strings(source, font, false, bytes, stats, error) && stats.keys == 2 && stats.fallbacks == 1, "bake translations and counted fallback");
    Strings table;
    check(table.open(bytes.data(), bytes.size()) && table.count() == 2 && table.fallbacks() == 1 &&
        std::string_view(table.font_name()) == "mono", "validated table and named font");
    check(table.find("go", Language::Ru) == "Go" && table.find("name", Language::Ru) == "\xD0\x96" &&
        table.find("name") == "A" && table.find("absent").empty() && table.find("bad key").empty(), "lookup, UTF-8, baked fallback and missing key");
    const auto original = bytes;
    std::reverse(source.begin(), source.end()); std::vector<uint8_t> reordered;
    check(bake_strings(source, font, false, reordered, stats, error) && reordered == bytes, "canonical order independent of TSV order");
    check(!bake_strings(source, font, true, bytes, stats, error) && bytes == original && stats.keys == 0 &&
        error.find("--strict") != std::string::npos, "strict rejects fallback and preserves output");
    const std::vector<StringSrc> same{{"go", "Go", "Go"}};
    check(bake_strings(same, font, true, bytes, stats, error) && stats.fallbacks == 0, "explicit identical translation is not fallback");
    const std::vector<StringSrc> golden{{"go", "Go", ""}};
    check(bake_strings(golden, font, false, bytes, stats, error) && bytes.size() == 84 &&
        asset::fnv1a(bytes.data(), bytes.size()) == 0xa7cab7b869e3f07cull, "independent Python struct/FNV wire golden");

    std::vector<StringSrc> parsed;
    check(parse_strings_tsv("key\ten\tru\r\ngo\t Go \t\r\nname\tA\t\xD0\x96\r\n", "fixture.tsv", parsed, error) &&
        parsed.size() == 2 && parsed[0].en == " Go " && parsed[0].ru.empty() && parsed[1].line == 3, "literal TSV, CRLF, spaces and empty translation");
    for (const std::string text : {"", "en\tkey\tru\na\tA\tA", "key\ten\tru\na\tA", "key\ten\tru\na\tA\tA\textra",
         "key\ten\tru\nbad key\tA\tA", "\xEF\xBB\xBFkey\ten\tru\na\tA\tA"})
        check(!parse_strings_tsv(text, "bad.tsv", parsed, error) && parsed.empty() && error.find("bad.tsv:") == 0, "malformed TSV refused without partial rows");
    for (const auto& row : std::vector<StringSrc>{{"empty", "", "A"}, {"utf8", "A", "\xC0\xAF"}, {"nul", std::string("A\0A", 3), "A"},
         {"bad key", "A", "A"}, {std::string(MAX_STRING_KEY + 1, 'a'), "A", "A"}})
        check(!bake_strings({&row, 1}, font, false, bytes, stats, error), "invalid text or key refused");
    auto missing = font;
    missing.glyphs.erase(std::remove_if(missing.glyphs.begin(), missing.glyphs.end(), [](const auto& glyph) { return glyph.codepoint == 0x416; }), missing.glyphs.end());
    const StringSrc alphabet{"grid.name", "A", "\xD0\x96"};
    check(!bake_strings({&alphabet, 1}, missing, false, bytes, stats, error) && error.find("grid.name") != std::string::npos &&
        error.find("U+0416") != std::string::npos, "alphabet coverage names key and missing code point");
    const std::vector<StringSrc> duplicate{{"same", "A", "A"}, {"same", "A", "A"}};
    check(!bake_strings(duplicate, font, false, bytes, stats, error), "duplicate real FNV key rejected");
    const StringSrc one{"one", "A", "A"}, two{"two", "A", "A"};
    const detail::IndexedString colliding[]{{7, &one}, {7, &two}};
    check(!detail::bake_indexed_strings(colliding, font, false, bytes, stats, error) && error.find("collision") != std::string::npos,
          "different source names with forced equal IDs rejected by actual indexed bake stage");

    check(bake_strings(golden, font, false, bytes, stats, error), "wire fixture restored");
    auto refused = [&](auto mutate) {
        auto bad = bytes; mutate(bad);
        check(!table.open(bad.data(), bad.size()) && table.count() == 0 && table.find("go").empty(), "malformed wire resets prior view");
    };
    refused([](auto& b) { b[0] ^= 1; });
    refused([](auto& b) { at<framework::core::SectionHeader>(b, 0).version = 0; });
    refused([](auto& b) { at<framework::core::SectionHeader>(b, 0).count = UINT32_MAX; });
    refused([](auto& b) { at<framework::core::SectionHeader>(b, 0).count = 0; });
    refused([](auto& b) { at<framework::core::SectionHeader>(b, 0).rows_offset = 25; });
    refused([](auto& b) { at<framework::core::SectionHeader>(b, 0).strings_offset = 56; });
    refused([](auto& b) { b.push_back(0); });
    refused([](auto& b) { at<StringRow>(b, 24).key = 0; });
    refused([](auto& b) { at<StringRow>(b, 24).id ^= 1; });
    refused([](auto& b) { at<StringRow>(b, 24).en = UINT32_MAX; });
    refused([](auto& b) { at<StringRow>(b, 24).en_size = UINT32_MAX; });
    refused([](auto& b) { at<StringRow>(b, 24).ru_size = 1; });
    refused([](auto& b) { at<StringRow>(b, 24).fallback = 2; });
    refused([](auto& b) { at<StringsMeta>(b, 56).reserved0 = 1; });
    refused([](auto& b) { at<StringsMeta>(b, 56).fallbacks = 0; });
    refused([](auto& b) { at<StringsMeta>(b, 56).font = 0; });
    refused([](auto& b) { b[78] = 0xff; }); // EN value, all offsets and lengths otherwise intact.
    refused([](auto& b) { b[78] = 0x1f; });
    refused([](auto& b) { b.back() = 'x'; });
    for (size_t n = 0; n < bytes.size(); ++n) check(!table.open(bytes.data(), n), "every truncation refused");
    std::vector<uint8_t> unaligned(bytes.size() + 1); std::copy(bytes.begin(), bytes.end(), unaligned.begin() + 1);
    check(!table.open(unaligned.data() + 1, bytes.size()), "unaligned base refused");
    const std::vector<StringSrc> pair{{"a", "A", "A"}, {"b", "A", "A"}};
    std::vector<uint8_t> two_rows;
    check(bake_strings(pair, font, true, two_rows, stats, error), "two complete row fixture");
    auto duplicated = two_rows;
    at<StringRow>(duplicated, 56) = at<StringRow>(duplicated, 24);
    check(!table.open(duplicated.data(), duplicated.size()), "duplicate IDs refused even with valid key hashes/counts");
    std::swap(at<StringRow>(two_rows, 24), at<StringRow>(two_rows, 56));
    check(!table.open(two_rows.data(), two_rows.size()), "descending valid IDs refused before binary lookup");

    framework::probe::in_hot = true;
    const bool control = framework::probe::control::plain_allocation() && framework::probe::control::aligned_allocation();
    framework::probe::in_hot = false;
    check(control && framework::probe::allocs >= 2, "allocation probe sees real plain/aligned allocations");
    framework::probe::allocs = 0; framework::probe::in_hot = true;
    const bool opened = table.open(bytes.data(), bytes.size());
    bool found = true;
    for (int i = 0; i < 1000; ++i) found = found && table.find("go", Language::Ru) == "Go";
    framework::probe::in_hot = false;
    check(opened && found && framework::probe::allocs == 0, "reader and lookup allocate nothing");
    std::puts(fails ? "strings: FAIL" : "strings: PASS TSV, coverage, strict/fallback, collision, wire validation and zero allocations");
    return fails ? 1 : 0;
}
