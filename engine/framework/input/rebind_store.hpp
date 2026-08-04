#pragma once
#include <string>
#include <vector>

#include "action_map.hpp"
#include "presets.hpp"

// Перебинды игрока — НАКЛАДКА поверх пресета, а не копия раскладки. Хранится только то, что игрок
// изменил сам, и хранится ПО ИМЕНИ действия: файл пережил бы добавление действия в манифест, а
// список индексов после такой правки молча переставил бы игроку половину кнопок.
//
// Сброс к пресету — это удаление накладки, отдельного «дефолтного» состояния не существует.
namespace framework::input {

struct Rebind {
    std::string action;
    uint32_t which = 0;          // номер альтернативного биндинга внутри действия
    ::input::Source src;
};

class RebindStore {
public:
    void set(const std::string& action, uint32_t which, ::input::Source src);
    bool get(const std::string& action, uint32_t which, ::input::Source& out) const;
    void reset(const std::string& action);
    void reset_all() { items_.clear(); }
    bool empty() const { return items_.empty(); }
    const std::vector<Rebind>& items() const { return items_; }

    // Заливка поверх уже привязанного пресета: ActionMap::rebind заменяет which-й биндинг.
    // Имя, которого нет в пресете, игнорируется — это старый файл, а не повод отказать в запуске.
    void apply(const PresetTable& table, uint32_t preset, ::input::ActionMap& map) const;

    // Текстовый формат того же вида, что манифест: `bind | action | which | source`. Имя пресета
    // пишется первой строкой — накладка от другой раскладки не должна применяться к этой.
    std::string serialize(const std::string& preset) const;
    bool parse(const std::string& text, std::string& preset_out);

    bool save(const std::string& path, const std::string& preset) const;
    bool load(const std::string& path, std::string& preset_out);

private:
    std::vector<Rebind> items_;
};

} // namespace framework::input
