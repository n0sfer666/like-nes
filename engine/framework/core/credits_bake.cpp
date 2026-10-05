#include "credits_bake.hpp"

#include <algorithm>

#include "credits_read.hpp"
#include "section_bake.hpp"
#include "text_fields.hpp"
#include "utf8_decode.hpp"

namespace framework::core {
namespace {

bool refuse(std::string& error, const std::string& message) {
    error = message;
    return false;
}

bool text_ok(const std::string& s) {
    for (const char c : s)
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) return false;
    return utf8_valid(s);
}

bool line_of(const std::vector<std::string>& f, std::vector<CreditSrc>& out, std::string& why) {
    if (f[0] == "credit") {
        if (f.size() != 5) return refuse(why, "a credit line is 'credit | pack | author | license | url'");
        out.push_back(CreditSrc{f[1], f[2], f[3], f[4], {}});
        return true;
    }
    if (f[0] != "attribution") return refuse(why, "unknown line kind '" + f[0] + "' (expected credit, attribution)");
    if (f.size() != 2 || f[1].empty()) return refuse(why, "an attribution line is 'attribution | text'");
    if (out.empty()) return refuse(why, "an attribution line comes before any credit line");
    if (!out.back().attribution.empty()) return refuse(why, "a credit has two attribution lines");
    out.back().attribution = f[1];
    return true;
}

bool credit_ok(const CreditSrc& c, std::string& why) {
    const std::string* fields[] = {&c.pack, &c.author, &c.license, &c.url};
    for (const std::string* f : fields)
        if (f->empty()) return refuse(why, "credit '" + c.pack + "' has an empty field");
    for (const std::string* f : fields)
        if (!text_ok(*f)) return refuse(why, "credit '" + c.pack + "' has a control character or broken UTF-8");
    if (!text_ok(c.attribution))
        return refuse(why, "credit '" + c.pack + "' has a control character or broken UTF-8");
    return true;
}

} // namespace

bool parse_credits(std::string_view text, const std::string& file, std::vector<CreditSrc>& out,
                   std::string& error) {
    out.clear();
    if (text.substr(0, 3) == "\xEF\xBB\xBF") return refuse(error, file + ":1: a byte order mark is not allowed");
    std::size_t start = 0;
    for (std::size_t line = 1; start < text.size(); ++line) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        const std::string raw = trim(std::string(text.substr(start, end - start)));
        start = end + 1;
        if (raw.empty() || raw[0] == '#') continue;
        std::string why;
        if (!line_of(split_fields(raw), out, why) || !credit_ok(out.back(), why))
            return refuse(error, file + ":" + std::to_string(line) + ": " + why);
        for (std::size_t i = 0; i + 1 < out.size(); ++i)
            if (out[i].pack == out.back().pack)
                return refuse(error, file + ":" + std::to_string(line) + ": pack '" + out.back().pack +
                                         "' is credited twice");
    }
    if (out.empty()) return refuse(error, file + ": no credit line");
    return true;
}

bool bake_credits(std::span<const CreditSrc> credits, std::vector<uint8_t>& out, std::string& error) {
    if (credits.empty()) return refuse(error, "no credit to bake");
    std::vector<CreditRow> rows(credits.size(), CreditRow{});
    SectionBuilder b(rows.size() * sizeof(CreditRow));
    for (std::size_t i = 0; i < credits.size(); ++i) {
        const CreditSrc& c = credits[i];
        if (!credit_ok(c, error)) return false;
        rows[i] = CreditRow{b.text(c.pack), b.text(c.author), b.text(c.license), b.text(c.url),
                            b.text(c.attribution)};
    }
    if (!b.finish(CREDITS_MAGIC, CREDITS_VERSION, static_cast<uint32_t>(rows.size()), rows.data(), out, error))
        return false;
    CreditTable probe;
    if (!probe.open(out.data(), out.size())) return refuse(error, "baked credits fail their own reader");
    return true;
}

} // namespace framework::core
