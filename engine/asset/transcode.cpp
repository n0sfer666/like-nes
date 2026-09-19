#include "transcode.hpp"

#include "basisu_transcoder.h"

namespace asset {

bool ktx2_to_bc7(const uint8_t* ktx2, uint32_t size, std::vector<uint8_t>& bc7,
                 uint32_t& w, uint32_t& h) {
    static bool inited = false;
    if (!inited) {
        basist::basisu_transcoder_init();
        inited = true;
    }
    basist::ktx2_transcoder t;
    if (!t.init(ktx2, size)) return false;
    if (!t.start_transcoding()) return false;
    w = t.get_width();
    h = t.get_height();
    basist::ktx2_image_level_info info;
    if (!t.get_image_level_info(info, 0, 0, 0)) return false;
    // Потолок стоит МЕЖДУ чужим числом и выделением памяти — в этом вся его суть: basisu принимает
    // шапку, объявляющую 65535×65535, и честно пересчитывает под неё 268 млн блоков, то есть
    // `resize` на 4 ГБ по восьми подменённым байтам шапки. Отказ ПОСЛЕ выделения ничего бы не спас,
    // поэтому улика в гейте — пустой буфер, а не возврат `false` (A·2·9).
    const uint64_t blocks = level_blocks(w, h, info.m_total_blocks);
    if (blocks == 0) return false;
    bc7.resize(static_cast<size_t>(blocks) * 16);
    return t.transcode_image_level(0, 0, 0, bc7.data(), static_cast<uint32_t>(blocks),
                                   basist::transcoder_texture_format::cTFBC7_RGBA);
}

} // namespace asset
