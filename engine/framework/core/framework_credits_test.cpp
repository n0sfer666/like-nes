#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "credits_bake.hpp"
#include "credits_read.hpp"
#include "section_format.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework::core;

const char* const GENERATED =
    "# Credits of the third-party assets, one pack per credit line.\n"
    "# Generated from LICENSES.toml by scripts/check_asset_licenses.py --write; do not edit.\n"
    "credit | monogram | Vin\xC3\xAD"
    "cius Men\xC3\xA9zio (@vmenezio) | CC0-1.0 | https://datagoblin.itch.io/monogram\n"
    "credit | warped-city | ansimuz | CC-BY-4.0 | https://opengameart.org/content/warped-city\n"
    "attribution | Warped City by ansimuz\n";

std::vector<uint8_t> bake(const char* text) {
    std::vector<CreditSrc> src;
    std::vector<uint8_t> out;
    std::string error;
    if (!parse_credits(text, "credits.txt", src, error) || !bake_credits(src, out, error))
        std::printf("    bake: %s\n", error.c_str());
    return out;
}

void test_round_trip() {
    const std::vector<uint8_t> bytes = bake(GENERATED);
    CreditTable t;
    check(t.open(bytes.data(), bytes.size()) && t.count() == 2, "generated credits bake and open");
    const Credit a = t.at(0);
    const Credit b = t.at(1);
    check(std::strcmp(a.pack, "monogram") == 0 && std::strcmp(a.author, "Vin\xC3\xAD"
                                                                         "cius Men\xC3\xA9zio (@vmenezio)") == 0,
          "pack and author keep their UTF-8 bytes");
    check(std::strcmp(a.license, "CC0-1.0") == 0 && std::strcmp(a.url, "https://datagoblin.itch.io/monogram") == 0,
          "license and url");
    check(a.attribution[0] == '\0', "a pack without attribution reads an empty one");
    check(std::strcmp(b.attribution, "Warped City by ansimuz") == 0, "attribution lands on the credit above it");
    check(t.at(2).pack[0] == '\0', "an index past the table reads empty, not out of bounds");
}

void refused(const char* text, const char* expected, const char* what) {
    std::vector<CreditSrc> src;
    std::string error;
    const bool ok = parse_credits(text, "credits.txt", src, error);
    const bool named = error.find(expected) != std::string::npos;
    if (ok || !named) std::printf("    got: %s\n", ok ? "accepted" : error.c_str());
    check(!ok && named, what);
}

void test_source_refusals() {
    refused("\xEF\xBB\xBF" "credit | a | b | c | d\n", "credits.txt:1: a byte order mark", "BOM");
    refused("", "credits.txt: no credit line", "an empty file");
    refused("# only a comment\n\n", "credits.txt: no credit line", "a file of comments");
    refused("# head\ncredit | a | b | c\n", "credits.txt:2: a credit line is", "a credit with four fields");
    refused("credit | a | b | c | d | e\n", "credits.txt:1: a credit line is", "a credit with six fields");
    refused("credit | a |  | c | d\n", "credits.txt:1: credit 'a' has an empty field", "an empty author");
    refused("thanks | a | b | c | d\n", "unknown line kind 'thanks'", "a foreign line kind");
    refused("attribution | x\ncredit | a | b | c | d\n", "credits.txt:1: an attribution line comes before",
            "attribution before any credit");
    refused("credit | a | b | c | d\nattribution | x\nattribution | y\n", "credits.txt:3: a credit has two",
            "two attributions of one credit");
    refused("credit | a | b | c | d\nattribution | x | y\n", "an attribution line is", "attribution with a bar");
    refused("credit | a | b | c | d\nattribution | \n", "an attribution line is", "an empty attribution");
    refused("credit | a | b | c | d\ncredit | a | e | f | g\n", "credits.txt:2: pack 'a' is credited twice",
            "the same pack twice");
    refused("credit | a | b\tc | d | e\n", "control character", "a tab inside a field");
    refused("credit | a | b\xD0 | d | e\n", "broken UTF-8", "a cut UTF-8 sequence");
    refused("credit | a | b | c | d\nattribution | \xC0\x80\n", "broken UTF-8", "an overlong attribution");
}

void patched(const std::vector<uint8_t>& bytes, std::size_t at, uint32_t value, const char* what) {
    std::vector<uint8_t> bad = bytes;
    std::memcpy(bad.data() + at, &value, sizeof(value));
    CreditTable r;
    check(!r.open(bad.data(), bad.size()), what);
}

void test_reader_refusals() {
    const std::vector<uint8_t> bytes = bake("credit | a | b | c | d\n");
    CreditTable t;
    check(t.open(bytes.data(), bytes.size()) && t.count() == 1, "one credit opens");
    SectionHeader head{};
    std::memcpy(&head, bytes.data(), sizeof(head));
    const std::size_t row = head.rows_offset;
    patched(bytes, offsetof(SectionHeader, count), 0, "a table of no credit");
    patched(bytes, row + offsetof(CreditRow, pack_offset), 0, "a pack pointing at the empty string");
    patched(bytes, row + offsetof(CreditRow, url_offset), head.total_size - head.strings_offset,
            "a url past the strings");
    patched(bytes, row + offsetof(CreditRow, attribution_offset), 0xFFFFFFFFu, "an attribution past the strings");
    patched(bytes, offsetof(SectionHeader, version), CREDITS_VERSION + 1, "a foreign version");
    check(!t.open(bytes.data(), bytes.size() - 1), "a cut table");
    const std::vector<uint8_t> roomy = bake("credit | a | b | c | dddddddddd\n");
    SectionHeader wide{};
    std::memcpy(&wide, roomy.data(), sizeof(wide));
    patched(roomy, offsetof(SectionHeader, strings_offset), wide.strings_offset + 4,
            "a gap between rows and strings, every offset still inside the shifted strings");
    check(!t.valid() && t.count() == 0 && t.at(0).pack[0] == '\0', "a refused open leaves the table empty");
}

void test_bake_refusals() {
    std::vector<uint8_t> out;
    std::string error;
    check(!bake_credits({}, out, error) && error == "no credit to bake", "nothing to bake");
    const CreditSrc empty_url{"a", "b", "c", "", ""};
    check(!bake_credits({&empty_url, 1}, out, error) && error.find("empty field") != std::string::npos,
          "the baker checks fields without the parser");
}

} // namespace

int main() {
    std::printf("credits: credits.txt grammar, section bake and reader\n");
    test_round_trip();
    test_source_refusals();
    test_reader_refusals();
    test_bake_refusals();
    std::printf("framework-credits: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
