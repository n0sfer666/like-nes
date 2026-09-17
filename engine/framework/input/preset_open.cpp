#include "presets.hpp"

#include <cstring>

namespace framework::input {
namespace {

template <typename T>
const T* view(const uint8_t* base, uint32_t offset, uint32_t count, std::size_t limit) {
    // Границы — единственная защита zero-parse формата: дальше по нему ходят указателями. Верхнего
    // предела мало: смещение проверяется снизу (иначе строки лягут поверх заголовка) и на
    // выравнивание (`reinterpret_cast` мимо границы = SIGBUS, как в `asset/bundle_view.cpp`), а
    // сумма считается в uint64 — в `size_t` на 32 битах `count * sizeof(T)` заворачивается.
    if (offset < sizeof(PresetHeader) || offset % alignof(T) != 0) return nullptr;
    if (static_cast<uint64_t>(offset) + static_cast<uint64_t>(count) * sizeof(T) > limit)
        return nullptr;
    return reinterpret_cast<const T*>(base + offset);
}

// Имя обязано кончаться нулём в пределах потолка. Проверка двойная по смыслу: смещение за концом
// блоба уводит `strcmp` в чужую память, а имя без потолка делает старт линейным по размеру секции —
// имена таблицы сравниваются ДРУГ С ДРУГОМ, и одно имя в мегабайт стоит столько же, сколько
// миллион строк (аудит #21, A·2·1b).
bool name_fits(const char* strings, uint32_t size, uint32_t offset) {
    if (offset >= size) return false;
    const uint32_t left = size - offset;
    const uint32_t window = left < MAX_NAME + 1 ? left : MAX_NAME + 1;
    return std::memchr(strings + offset, '\0', window) != nullptr;
}

bool names_fit(const PresetHeader& h, const PresetRow* presets, const ActionRow* actions,
               const AxisRow* axes, const PadRow* pads, const char* strings, uint32_t size) {
    for (uint32_t i = 0; i < h.preset_count; ++i)
        if (!name_fits(strings, size, presets[i].name_offset)) return false;
    for (uint32_t i = 0; i < h.action_count; ++i)
        if (!name_fits(strings, size, actions[i].name_offset)) return false;
    for (uint32_t i = 0; i < h.axis_count; ++i)
        if (!name_fits(strings, size, axes[i].name_offset)) return false;
    // Имена падов — те же имена блоба: `profile_for` сравнивает их с именем устройства, а
    // `match_offset` ещё и ищет в нём подстроку. Смещение 0 законно и значит «не сопоставлять по
    // имени»: в блобе по нулю лежит пустая строка, и потолок она проходит (аудит #21, ревью A·2).
    for (uint32_t i = 0; i < h.pad_count; ++i)
        if (!name_fits(strings, size, pads[i].name_offset) ||
            !name_fits(strings, size, pads[i].match_offset))
            return false;
    return true;
}

} // namespace

bool PresetTable::open(const void* data, std::size_t size) {
    header_ = nullptr;
    if (data == nullptr || size < sizeof(PresetHeader)) return false;
    const auto* base = static_cast<const uint8_t*>(data);
    const auto* h = reinterpret_cast<const PresetHeader*>(base);
    if (std::memcmp(h->magic, PRESET_MAGIC, sizeof(h->magic)) != 0) return false;
    if (h->version != PRESET_VERSION || h->total_size > size) return false;
    // Потолок пресетов стоит до любого обхода: за ним идёт проверка имён, линейная по строкам
    // ВСЕХ пресетов, и без потолка отказа не будет — будет работа (аудит #21, ревью A·2).
    if (h->preset_count > MAX_PRESETS) return false;

    // Потолок таблиц — ИТОГ заголовка, а не размер буфера: буфер бывает длиннее секции (общий
    // блоб бандла, читалка с запасом), и по `size` таблица законно уезжала бы в чужие байты, а
    // `strings_size_` ниже считается уже от итога — два разных потолка в одном читателе
    // расходятся молча (аудит #21, ревью A·2).
    presets_ = view<PresetRow>(base, h->presets_offset, h->preset_count, h->total_size);
    actions_ = view<ActionRow>(base, h->actions_offset, h->action_count, h->total_size);
    axes_ = view<AxisRow>(base, h->axes_offset, h->axis_count, h->total_size);
    bindings_ = view<BindingRow>(base, h->bindings_offset, h->binding_count, h->total_size);
    pads_ = view<PadRow>(base, h->pads_offset, h->pad_count, h->total_size);
    if (presets_ == nullptr || actions_ == nullptr || axes_ == nullptr || bindings_ == nullptr ||
        pads_ == nullptr)
        return false;
    // Потолок строк осей у ЧИТАТЕЛЯ: срез пресета честно лежит в таблице осей, и отбить пресет,
    // растянутый на строки соседа, может только он (аудит #21, A·2·1b).
    for (uint32_t i = 0; i < h->preset_count; ++i)
        if (presets_[i].axis_count > MAX_AXIS_ROWS) return false;
    // Блоб имён — единственная таблица мимо `view<T>()`: у него нет ни типа, ни счёта строк,
    // поэтому обе границы ставятся здесь руками. Нижняя не декоративная: со смещением 0 имена
    // начинаются на магии и версии, `strings_size_` захватывает заголовок целиком, а имя длиной
    // ноль указывает в `magic` (аудит #21, ревью A·2).
    if (h->strings_offset < sizeof(PresetHeader) || h->strings_offset >= h->total_size)
        return false;

    strings_ = reinterpret_cast<const char*>(base + h->strings_offset);
    strings_size_ = h->total_size - h->strings_offset;
    if (strings_[strings_size_ - 1] != '\0') return false;   // обход имён обязан упереться в ноль
    if (!names_fit(*h, presets_, actions_, axes_, pads_, strings_, strings_size_)) return false;
    header_ = h;
    return true;
}

} // namespace framework::input
