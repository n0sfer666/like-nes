#include "preset_parse.hpp"

namespace framework::input {

// Блоб имён: одинаковые строки манифеста делят одно смещение, а смещение 0 занято пустой строкой и
// значит «имени нет». Дедупликация здесь не про размер таблицы, а про сравнение имён по смещению.
uint32_t PresetStrings::add(const std::string& s) {
    if (s.empty()) return 0;
    const auto it = seen.find(s);
    if (it != seen.end()) return it->second;
    const uint32_t off = static_cast<uint32_t>(data.size());
    data.insert(data.end(), s.begin(), s.end());
    data.push_back('\0');
    seen.emplace(s, off);
    return off;
}

} // namespace framework::input
