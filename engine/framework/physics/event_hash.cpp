#include "event_hash.hpp"

#include "hash_mix.hpp"

namespace framework::physics {

uint64_t event_hash_seed() {
    return FNV_OFFSET;
}

uint64_t mix_events(uint64_t seed, const std::vector<ContactEvent>& events) {
    uint64_t h = seed;
    // Длина кадра входит в свёртку ПЕРВОЙ. Без неё поток «два события, ноль» и поток «ноль, два
    // события» сворачиваются в одно и то же число, а это разные прогоны: событие привязано к кадру,
    // и кадр, на котором оно случилось, — часть ответа.
    mix(h, static_cast<uint32_t>(events.size()));
    for (const ContactEvent& e : events) {
        mix(h, e.key_a);
        mix(h, e.key_b);
        // Фаза и признак триггера — в одно слово: оба маленькие, оба про род события, и раздельная
        // свёртка двух почти пустых чисел только удлиняет сравнение.
        mix(h, static_cast<uint32_t>(e.phase) | (e.trigger ? 0x100u : 0u));
        // Геометрия сворачивается ЦЕЛИКОМ, включая нулевую у фазы `End`. Свернуть только у begin и
        // stay значило бы получить голден, чья длина зависит от фаз, — и перестановка фаз, не
        // меняющая набор, прошла бы мимо него.
        mix(h, static_cast<uint32_t>(e.normal.x.raw));
        mix(h, static_cast<uint32_t>(e.normal.y.raw));
        mix(h, static_cast<uint32_t>(e.point.x.raw));
        mix(h, static_cast<uint32_t>(e.point.y.raw));
        mix(h, static_cast<uint32_t>(e.penetration.raw));
    }
    return h;
}

} // namespace framework::physics
