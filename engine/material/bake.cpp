#include "bake.hpp"

#include <cstdio>
#include <cstring>

#include "../asset/hash.hpp"
#include "bake_rows.hpp"

namespace mat {
namespace {

// Пул строк с дедупликацией: имя параметра повторяется в каждом инстансе, и хранить его копией на
// строку значит платить за наследование ровно тем, ради чего оно и заводилось.
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

// GUID шейдера и текстуры считается от ЛОГИЧЕСКОГО ИМЕНИ тем же fnv1a, что и `guid_of` в
// `tools/assetc`: материал ссылается на ассет, испечённый соседним пекарем, и вторая формула
// разошлась бы с ним молча — ссылка просто не нашлась бы в рантайме.
uint64_t guid(const std::string& name) { return asset::fnv1a(name.data(), name.size()); }

} // namespace

bool bake_materials(const std::string& text, std::vector<uint8_t>& out, BakeError& err) {
    std::vector<MaterialSpec> specs;
    if (!parse_materials(text, specs, err)) return false;

    Strings strings;
    std::vector<MaterialRow> materials;
    std::vector<ParamRow> params;
    std::vector<TextureRow> textures;
    materials.reserve(specs.size());

    for (const MaterialSpec& m : specs) {
        MaterialRow row{};
        row.shader_guid = guid(m.shader);
        row.name_off = strings.add(m.name);
        row.shader_off = strings.add(m.shader);
        row.param_first = static_cast<uint32_t>(params.size());
        row.param_count = static_cast<uint16_t>(m.params.size());
        row.texture_first = static_cast<uint32_t>(textures.size());
        row.texture_count = static_cast<uint16_t>(m.textures.size());
        row.base = m.base < 0 ? NO_BASE : static_cast<uint16_t>(m.base);
        row.blend = static_cast<uint8_t>(m.blend);
        materials.push_back(row);

        for (const ParamSpec& p : m.params) {
            ParamRow pr{};
            pr.name_off = strings.add(p.name);
            std::memcpy(pr.value, p.value, sizeof(pr.value));
            pr.type = static_cast<uint8_t>(p.type);
            pr.unit = static_cast<uint8_t>(p.unit);
            pr.slot = p.slot;
            params.push_back(pr);
        }
        for (const TexSpec& t : m.textures) {
            TextureRow tr{};
            tr.guid = guid(t.asset);
            tr.name_off = strings.add(t.name);
            tr.binding = t.binding;
            textures.push_back(tr);
        }
    }

    TableHeader h{};
    std::memcpy(h.magic, TABLE_MAGIC, 4);
    h.version = TABLE_VERSION;
    h.material_count = static_cast<uint32_t>(materials.size());
    h.param_count = static_cast<uint32_t>(params.size());
    h.texture_count = static_cast<uint32_t>(textures.size());
    h.materials_offset = sizeof(TableHeader);
    h.params_offset = h.materials_offset + h.material_count * sizeof(MaterialRow);
    h.textures_offset = h.params_offset + h.param_count * sizeof(ParamRow);
    h.strings_offset = h.textures_offset + h.texture_count * sizeof(TextureRow);
    h.total_size = h.strings_offset + static_cast<uint32_t>(strings.bytes().size());

    out.clear();
    out.reserve(h.total_size);
    append(out, h);
    for (const MaterialRow& r : materials) append(out, r);
    for (const ParamRow& r : params) append(out, r);
    for (const TextureRow& r : textures) append(out, r);
    out.insert(out.end(), strings.bytes().begin(), strings.bytes().end());
    return true;
}

bool bake_materials_file(const std::string& path, std::vector<uint8_t>& out, BakeError& err) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        err.line = 0;
        err.message = "cannot open " + path;
        return false;
    }
    std::string text;
    char buf[4096];
    for (;;) {
        const std::size_t n = std::fread(buf, 1, sizeof(buf), f);
        if (n == 0) break;
        text.append(buf, n);
    }
    std::fclose(f);
    return bake_materials(text, out, err);
}

} // namespace mat
