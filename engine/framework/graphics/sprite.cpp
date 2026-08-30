#include "sprite.hpp"

#include <algorithm>

namespace framework::graphics {

uint64_t sprite_key(const Sprite& s, uint32_t submitted) {
    const uint64_t layer = static_cast<uint64_t>(static_cast<uint32_t>(s.layer) + 0x8000u) & 0xffffu;
    return (layer << 48) | (static_cast<uint64_t>(s.material) << 32) | submitted;
}

SpriteList::SpriteList(Sprite* sprites, uint64_t* keys, uint32_t capacity)
    : sprites_(sprites), keys_(keys),
      capacity_(sprites != nullptr && keys != nullptr ? capacity : 0) {}

void SpriteList::clear() {
    count_ = 0;
    dropped_ = 0;
}

void SpriteList::push(const Sprite& s) {
    if (count_ >= capacity_) {
        ++dropped_;
        return;
    }
    keys_[count_] = sprite_key(s, count_);
    sprites_[count_++] = s;
}

const Sprite& SpriteList::drawn(uint32_t i) const { return sprites_[keys_[i] & 0xffffffffu]; }

uint32_t SpriteList::build(Batch* out, uint32_t max_batches) {
    if (count_ == 0) return 0;
    // `std::sort`, а не `std::stable_sort`: второй выделяет буфер под слияние, то есть ходит в кучу
    // прямо на пути, где гейт 8 требует нуля. Стабильность обеспечена номером подачи в ключе, и
    // цена этого решения ровно одна — порядок обязан быть полным, что и утверждает гейт.
    std::sort(keys_, keys_ + count_);
    // Нечего вернуть — значит, нечего и нарисовать: спрайты считаются потерянными ЗДЕСЬ, а не
    // молчат до кадра. «Ноль батчей» и «ноль спрайтов» снаружи неразличимы, и именно этим
    // различением меряется гейт 6.
    if (out == nullptr || max_batches == 0) {
        dropped_ += count_;
        return 0;
    }
    uint32_t n = 0;
    // Батч рвётся по смене МАТЕРИАЛА, а не слоя: слои уже развели спрайты в порядке, и два соседних
    // отрезка одного материала из разных слоёв рисуются одним вызовом, ничего не переставляя. Резать
    // их порознь значило бы платить draw-call за границу, которой на экране не видно.
    uint16_t material = static_cast<uint16_t>((keys_[0] >> 32) & 0xffffu);
    out[0] = Batch{0, 1, material};
    for (uint32_t i = 1; i < count_; ++i) {
        const uint16_t m = static_cast<uint16_t>((keys_[i] >> 32) & 0xffffu);
        if (m == material) {
            ++out[n].count;
            continue;
        }
        if (n + 1 >= max_batches) {
            dropped_ += count_ - i;
            return n + 1;
        }
        material = m;
        out[++n] = Batch{i, 1, material};
    }
    return n + 1;
}

} // namespace framework::graphics
