#include "bake_rows.hpp"

namespace ach {
namespace {

std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return std::string();
    const std::size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t bar = s.find('|', start);
        out.push_back(trim(s.substr(start, bar == std::string::npos ? bar : bar - start)));
        if (bar == std::string::npos) return out;
        start = bar + 1;
    }
}

bool fail(BakeError& err, int line, const std::string& msg) {
    err.line = line;
    err.message = msg;
    return false;
}

bool parse_u64(const std::string& s, uint64_t& out) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) return false;
    out = 0;
    for (char c : s) {
        const uint64_t d = static_cast<uint64_t>(c - '0');
        if (out > (~0ull - d) / 10) return false;
        out = out * 10 + d;
    }
    return true;
}

bool declared(const std::vector<std::string>& keys, Id id) {
    for (const std::string& k : keys) {
        if (hash_key(k.c_str()) == id) return true;
    }
    return false;
}

bool parse_ach(const std::vector<std::string>& f, int line_no, const std::vector<std::string>& stats,
               std::vector<Row>& rows, BakeError& err) {
    if (f.size() != 8) {
        return fail(err, line_no, "ach | key | kind | stat | target | flags | name | desc");
    }
    Row r;
    r.line = line_no;
    r.key = f[1];
    r.name = f[6];
    r.desc = f[7];
    r.def = Def{};
    if (r.key.empty()) return fail(err, line_no, "empty key");
    r.def.id = hash_key(r.key.c_str());

    if (f[2] == "progress") {
        r.def.kind = static_cast<uint32_t>(Kind::Progress);
        if (f[3].empty()) return fail(err, line_no, "progress needs a stat");
        if (!parse_u64(f[4], r.def.target) || r.def.target == 0) {
            return fail(err, line_no, "progress needs a positive target");
        }
        r.def.stat = hash_key(f[3].c_str());
        if (!declared(stats, r.def.stat)) return fail(err, line_no, "undeclared stat " + f[3]);
    } else if (f[2] == "bool") {
        r.def.kind = static_cast<uint32_t>(Kind::Boolean);
        if (!f[3].empty() || (!f[4].empty() && f[4] != "0")) {
            return fail(err, line_no, "bool must not bind a stat or target");
        }
    } else {
        return fail(err, line_no, "kind must be 'bool' or 'progress'");
    }

    if (f[5] == "hidden") {
        r.def.flags = FLAG_HIDDEN;
    } else if (!f[5].empty() && f[5] != "-") {
        return fail(err, line_no, "flags must be '-' or 'hidden'");
    }
    for (const Row& prev : rows) {
        if (prev.def.id == r.def.id) {
            return fail(err, line_no, "duplicate achievement " + r.key + " (line " +
                                          std::to_string(prev.line) + ")");
        }
    }
    rows.push_back(r);
    return true;
}

} // namespace

bool parse_manifest(const std::string& text, std::vector<Row>& rows,
                    std::vector<std::string>& stat_keys, BakeError& err) {
    rows.clear();
    stat_keys.clear();
    std::vector<std::pair<int, std::vector<std::string>>> records;
    std::size_t pos = 0;
    int line_no = 0;

    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::string raw = text.substr(pos, nl == std::string::npos ? nl : nl - pos);
        pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
        ++line_no;

        const std::size_t comment = raw.find('#');
        const std::string line = trim(comment == std::string::npos ? raw : raw.substr(0, comment));
        if (line.empty()) continue;

        std::vector<std::string> f = split(line);
        if (f[0] != "stat" && f[0] != "ach") {
            return fail(err, line_no, "unknown record '" + f[0] + "'");
        }
        records.emplace_back(line_no, std::move(f));
    }

    for (const auto& rec : records) {
        const std::vector<std::string>& f = rec.second;
        if (f[0] != "stat") continue;
        if (f.size() != 2 || f[1].empty()) return fail(err, rec.first, "stat | <key>");
        if (declared(stat_keys, hash_key(f[1].c_str()))) {
            return fail(err, rec.first, "duplicate stat " + f[1]);
        }
        stat_keys.push_back(f[1]);
    }
    for (const auto& rec : records) {
        if (rec.second[0] != "ach") continue;
        if (!parse_ach(rec.second, rec.first, stat_keys, rows, err)) return false;
    }
    err.line = 0;
    err.message.clear();
    return true;
}

} // namespace ach
