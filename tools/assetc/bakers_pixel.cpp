#include "bakers.hpp"

#include "baker_guid.hpp"
#include "format.hpp"

namespace asset::bakers {

// Raw, а не Zstd: пиксель-арт жмётся хорошо, но секция грузится в GPU как есть, а распаковка
// стоила бы каждого старта уровня ради экономии диска, которую бюджет 30 МиБ уже держит.
bool pixel(const std::vector<uint8_t>& png, const char* name, std::vector<AssetInput>& out,
           std::string& error) {
    std::vector<uint8_t> rgba;
    uint32_t w = 0, h = 0;
    if (!codec::png_to_rgba8(png, rgba, w, h, error)) return false;
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Texture;
    a.codec = Codec::Raw;
    a.residency = Residency::Stream;
    a.uncompressed_size = static_cast<uint32_t>(rgba.size());
    a.tex_w = w;
    a.tex_h = h;
    a.tex_format = TEX_FORMAT_RGBA8_UNORM;
    a.payload = std::move(rgba);
    out.push_back(std::move(a));
    return true;
}

} // namespace asset::bakers
