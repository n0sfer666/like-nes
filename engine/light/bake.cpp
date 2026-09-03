#include "bake.hpp"

#include <cstdint>
#include <cstring>

#include "../platform/platform_fs.hpp"

namespace light {
namespace {

// Пул строк с дедупликацией — тот же приём, что у пекаря материалов: имя источника повторяется
// редко, но секция обязана быть терминирована, а смещения — лежать внутри неё.
class Strings {
public:
    uint32_t add(const std::string& s) {
        for (const Entry& e : entries_)
            if (e.text == s) return e.offset;
        const uint32_t off = static_cast<uint32_t>(bytes_.size());
        bytes_.insert(bytes_.end(), s.begin(), s.end());
        bytes_.push_back('\0');
        entries_.push_back(Entry{s, off});
        return off;
    }
    const std::vector<uint8_t>& bytes() const { return bytes_; }

private:
    struct Entry {
        std::string text;
        uint32_t offset;
    };
    std::vector<Entry> entries_;
    std::vector<uint8_t> bytes_;
};

template <typename T>
void append(std::vector<uint8_t>& out, const T& v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

} // namespace

bool bake_lights(const std::string& text, std::vector<uint8_t>& out, BakeError& err) {
    LightSet set;
    if (!parse_lights(text, set, err)) return false;
    const std::vector<LightSpec>& specs = set.lights;

    Strings strings;
    std::vector<LightRow> rows;
    rows.reserve(specs.size());
    for (const LightSpec& l : specs) {
        LightRow r{};
        r.name_off = strings.add(l.name);
        r.kind = static_cast<uint8_t>(l.kind);
        std::memcpy(r.pos, l.pos, sizeof(r.pos));
        std::memcpy(r.dir, l.dir, sizeof(r.dir));
        std::memcpy(r.color, l.color, sizeof(r.color));
        r.height = l.height;
        r.intensity = l.intensity;
        r.radius = l.radius;
        r.shadow = l.shadow;
        rows.push_back(r);
    }

    TableHeader h{};
    std::memcpy(h.magic, TABLE_MAGIC, 4);
    h.version = TABLE_VERSION;
    std::memcpy(h.ambient, set.ambient, sizeof(h.ambient));
    h.light_count = static_cast<uint32_t>(rows.size());
    h.lights_offset = sizeof(TableHeader);
    // Смещения считаются в `size_t` и лишь потом сужаются: арифметика раскладки, переполнившаяся в
    // uint32, дала бы не отказ, а согласованный сам с собой заголовок — и читатель принял бы его.
    const std::size_t strings_at = h.lights_offset + rows.size() * sizeof(LightRow);
    const std::size_t total = strings_at + strings.bytes().size();
    if (total > UINT32_MAX) {
        err.line = 0;
        err.message = "table does not fit a 32-bit layout";
        return false;
    }
    h.strings_offset = static_cast<uint32_t>(strings_at);
    h.total_size = static_cast<uint32_t>(total);

    out.clear();
    out.reserve(h.total_size);
    append(out, h);
    for (const LightRow& r : rows) append(out, r);
    out.insert(out.end(), strings.bytes().begin(), strings.bytes().end());
    return true;
}

bool bake_lights_file(const std::string& path, std::vector<uint8_t>& out, BakeError& err) {
    // Через платформенный шов, а не `fopen`: на MSVC он депрекирован, и `/W4 /WX` раннера валит
    // сборку там, где clang молчит.
    std::string text;
    if (!platform::read_text(path, text)) {
        err.line = 0;
        err.message = "cannot open " + path;
        return false;
    }
    return bake_lights(text, out, err);
}

} // namespace light
