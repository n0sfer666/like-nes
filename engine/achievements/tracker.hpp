#pragma once
#include "registry.hpp"
#include "state.hpp"
#include "../asset/hash.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ach {

struct Event {
    Id id;
    uint64_t tick;
};

class Tracker {
public:
    explicit Tracker(const Registry& reg);

    void set_tick(uint64_t tick) { tick_ = tick; }
    uint64_t tick() const { return tick_; }

    void set_stat(Id stat, uint64_t value);
    void add_stat(Id stat, uint64_t delta);
    void unlock(Id ach);

    uint64_t stat(Id stat) const;
    bool unlocked(Id ach) const;
    std::size_t unlocked_count() const { return unlocked_count_; }

    const std::vector<Event>& events() const { return events_; }
    void drain(std::size_t n);

    const std::vector<Id>& stat_ids() const { return stat_ids_; }
    const std::vector<uint64_t>& stat_values() const { return stat_values_; }
    const std::vector<Id>& ach_ids() const { return ach_ids_; }
    const std::vector<uint8_t>& ach_flags() const { return ach_flags_; }

    // Потолок на КАЖДЫЙ список переноса (аудит #21, A·2·4). Перенос ищет дубликат линейно по уже
    // накопленному вектору, то есть стоит квадрат по числу неизвестных записей, а ограничен он был
    // только размером файла: сейв на 8 МиБ из миллиона неизвестных идентификаторов — ≈5,5·10¹¹
    // сравнений на ЗАГРУЗКЕ, минуты-часы CPU при полностью корректном по формату файле, и хеш
    // целостности такому файлу сходится (fnv не секрет, атакующий считает его сам). Перенос
    // существует ради ЧУЖИХ идентификаторов, и разумное их число — сотни, а не миллион.
    //
    // Потолок считается ЗА ВСЁ ВРЕМЯ ЖИЗНИ трекера, а не за один restore(): перенос не очищается
    // нигде, и трекер, набравший полный перенос первой загрузкой, на второй отбросит неизвестные
    // записи нового снимка целиком — включая те, которых в переносе ещё нет. Чистить перенос на
    // входе restore() значило бы менять поведение повторной загрузки, а это отдельный вопрос.
    static constexpr std::size_t MAX_CARRIED = 4096;

    void snapshot(Snapshot& out) const;
    // Возвращает число записей, каталогу неизвестных — ВСЕХ, включая отброшенные потолком: вопрос
    // «каталог урезан?» и вопрос «сколько из этого потеряно?» разные, и второй спрашивается у
    // dropped_count(). Неизвестные не применяются, но и не теряются до потолка — snapshot() вернёт
    // их дословно, поэтому сохранение при урезанном каталоге не стирает прогресс.
    std::size_t restore(const Snapshot& snap);
    std::size_t carried_count() const { return carried_.stats.size() + carried_.unlocked.size(); }
    // Сколько неизвестных записей потолок отбросил за всё время жизни объекта. Молчаливая потеря
    // прогресса неотличима от его отсутствия, поэтому число обязано быть спрашиваемым.
    std::size_t dropped_count() const { return dropped_; }

    uint64_t hash() const;
    uint64_t progress_hash() const;

private:
    std::size_t stat_slot(Id stat) const;
    std::size_t ach_slot(Id ach) const;
    void evaluate(std::size_t def_index, uint64_t value);
    void emit(std::size_t slot);
    void carry_stat(const StatRecord& rec);
    void carry_unlocked(Id id);

    uint64_t tick_ = 0;
    uint64_t event_digest_ = asset::FNV_OFFSET;
    std::size_t unlocked_count_ = 0;
    std::size_t dropped_ = 0;

    std::vector<Id> stat_ids_;
    std::vector<uint64_t> stat_values_;
    std::vector<Id> ach_ids_;
    std::vector<uint8_t> ach_flags_;
    std::vector<Def> defs_;
    std::vector<std::vector<uint32_t>> by_stat_;
    std::vector<Event> events_;
    Snapshot carried_;
};

} // namespace ach
