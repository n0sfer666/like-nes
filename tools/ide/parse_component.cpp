// Разбор одной строки компонента: причины 9–14 из списка в `serialize.hpp`. Почему отдельным
// файлом от текстов — в `parse_component.hpp`.
#include "parse_component.hpp"
#include "serialize.hpp"

namespace ide::detail {
namespace {

// Исходов у применения компонента ЧЕТЫРЕ: имени нет в таблице — опечатка в ИМЕНИ; текст не json —
// опечатка в СИНТАКСИСЕ; json разобрался, а значение вышло не тем, что пишет формат, — опечатка в
// ЧЛЕНЕ. Чинятся они в разных местах строки, поэтому и советы разные (аудит #21, A·2·8).
enum class Applied { Ok, Unknown, BadJson, NotCanonical };

// Пробелы не значат ничего: flecs пишет `{"x":1, "y":2}`, а правленный руками файл обычно пишет
// `{"x":1,"y":2}`, и отказ за это был бы отказом за форматирование, а не за содержание.
// Вычерк ОГУЛЬНЫЙ, вместе с пробелами внутри литералов, и это не упущение: сверка ниже сравнивает
// текст с переизлучением ЭТОГО ЖЕ текста, а внутри литерала переизлучение сохраняет байты как
// есть — значит вычерк симметричен и значение («two words») пережить его обязано на обеих
// сторонах. Прежняя версия умела различать литералы; мутационный прогон показал, что отличить её
// от этой нечем ни одной фикстурой, то есть те десять строк были кодом без проверки (A·2·8).
std::string squeeze(const std::string& s) {
    std::string out;
    for (char c : s)
        if (c != ' ' && c != '\t') out += c;
    return out;
}

template <typename T>
Applied set_from_json(flecs::world& w, flecs::entity e, const std::string& json) {
    T v{};
    // Возврат `from_json` до сих пор выбрасывался: на битом значении `v` оставалась нулевой, и
    // сущность получала `T{}` под видом прочитанного из файла.
    if (!w.from_json<T>(&v, json.c_str())) return Applied::BadJson;
    // Но не-NULL означает «текст БЫЛ json», а не «значение прочиталось»: неизвестный член
    // (`{"n":…}` вместо `{"value":…}`), пропущенный член и `{}` принимались молча, и компонент
    // снова оставался `T{}`. Ровно этот дефект находка и закрывает — позиция (0,0), неотличимая
    // от прочитанной. Сверяем с тем, что на этом значении НАПИСАЛ БЫ сам сериализатор: одна
    // проверка на все типы, и она же ловит хвост после json (ревью аудита #21, A·2·8).
    // `to_json` отдаёт `flecs::string`, и её `c_str()` бывает NULL, когда `ecs_ptr_to_json` не
    // справился: `std::string(nullptr)` — это UB, а не пустая строка (ревью аудита #21, A·2·8).
    const flecs::string back = w.to_json<T>(&v);
    if (!back.c_str() || squeeze(back.c_str()) != squeeze(json)) return Applied::NotCanonical;
    e.set<T>(v);
    return Applied::Ok;
}

Applied apply_component(flecs::world& w, flecs::entity e,
                        const std::string& name, const std::string& json) {
    if (name == "Name") return set_from_json<Name>(w, e, json);
    if (name == "Parent") return set_from_json<Parent>(w, e, json);
    if (name == "Position") return set_from_json<Position>(w, e, json);
    if (name == "Velocity") return set_from_json<Velocity>(w, e, json);
    return Applied::Unknown;
}

// Одна последовательность UTF-8, начинающаяся в `i` и не выходящая за `end`: её длина или 0, если
// последовательности там нет. Строгость та же, что у проверки в гейте, и не лишняя: приняв
// пересортицу (`C0 80` вместо `00`), суррогат или точку выше U+10FFFF, цитата протащила бы в панель
// байты, которые её же декодер разберёт иначе. Ноль возвращается и на управляющих байтах — они
// печатным текстом не являются: ESC из чужого файла уезжает в терминал командой, а не символом.
// Вызывающему обе беды чинятся одинаково, поэтому исход и один (ревью аудита #21, A·2·8).
size_t utf8_seq(const std::string& s, size_t i, size_t end) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3
                                                             : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (len == 0 || i + len > end) return 0;
    uint32_t cp = len == 1 ? c : (c & (0xFFu >> (len + 1)));
    for (size_t k = 1; k < len; ++k) {
        const unsigned char t = static_cast<unsigned char>(s[i + k]);
        if ((t & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (t & 0x3F);
    }
    static const uint32_t MIN[5] = {0, 0, 0x80, 0x800, 0x10000};
    if (cp < MIN[len] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
    if (cp < 0x20 || cp == 0x7F) return 0;
    return len;
}

} // namespace

// Диагностика уезжает в лог и в панель редактора, а БАЙТЫ в неё кладёт тот, кто прислал файл.
// Отсюда две разные заботы. Первая — ДЛИНА: однострочный сейв на мегабайт стал бы мегабайтной
// строкой ошибки, поэтому цитата режется, и граница отодвигается назад по продолжающим байтам —
// разрезанная посередине последовательность это мусор там, где ждали имя объекта.
//
// Вторая — САМИ байты, и прежде она была упущена: в файле их может не быть в UTF-8 вовсе (latin-1
// из чужого редактора, оборванный хвост, просто бинарник не той породы), и такая цитата рвёт
// кодировку ВСЕЙ строки, куда её вставили, — панель показывает кашу вместо причины отказа. А
// управляющие байты вдобавок уезжают в терминал как есть, и ESC из чужого файла там команда, а не
// символ. Поэтому всё, что не печатный UTF-8, заменяется на `?`: цитата обязана остаться ЧИТАЕМОЙ,
// и это дороже её точности — по ней ищут правку глазами (ревью аудита #21, A·2·8).
std::string quoted(const std::string& s) {
    constexpr size_t MAX = 64;
    size_t cut = s.size();
    if (cut > MAX) {
        cut = MAX;
        while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
        // Откат, упёршийся в ноль, отдавал цитату `'...'` — ровно ту пустую строку, против которой
        // заведён санитайзер ниже. Возврат к MAX ничего валидного не рассечёт: раз все 64 байта
        // продолжающие, целой последовательности среди них нет ни одной, и каждый уйдёт в `?`
        // (ревью аудита #21, A·2·8).
        if (cut == 0) cut = MAX;
    }
    std::string out = "'";
    for (size_t i = 0; i < cut;) {
        const size_t len = utf8_seq(s, i, cut);
        if (len == 0) { out += '?'; ++i; }
        else { out.append(s, i, len); i += len; }
    }
    out += cut < s.size() ? "...'" : "'";
    return out;
}

// Номер строки идёт ПЕРВЫМ и считается по тексту, который РАЗБИРАЮТ. Тексты эти разные: файл сцены
// чинят в редакторе, и номер там ровно этот, а снимок undo живёт в памяти — его номер в файле не
// значит ничего, поэтому `where` и говорит, о каком тексте речь (ревью аудита #21, A·2·8).
std::string at(const char* where, size_t no, const std::string& msg) {
    return std::string(where) + std::to_string(no) + ": " + msg;
}

// Разбор строки `C <Name> <json>` — общий у полной загрузки и у undo: ТЕЛО такой строки в обоих
// текстах одно, и два разных разбора разошлись бы молча на первой же правке формата. Диспетчер
// строк общим НЕ является и не притворяется: файл сцены знает `E`, `#` и шапку, снимок — `C`.
bool apply_line(flecs::world& w, flecs::entity e, uint64_t guid, const std::string& line,
                const char* where, size_t no, std::set<std::string>& seen, std::string* why) {
    const std::string who = "entity " + std::to_string(guid) + ", ";
    size_t sp = line.find(' ', 2);
    if (sp == std::string::npos) {
        if (why) *why = at(where, no, who + "component line has no value: " + quoted(line));
        return false;
    }
    const std::string name = line.substr(2, sp - 2);
    // Два пробела подряд давали пустое имя, и диагностика выходила как `component : unknown
    // component` — одна из трёх обещанных координат пустая, а самой строки в сообщении нет.
    if (name.empty()) {
        if (why) *why = at(where, no, who + "component line has no name: " + quoted(line));
        return false;
    }
    // Дубль имени у ОДНОЙ сущности: второй `set<T>` молча перетирал первое значение, файл с двумя
    // `C Position` грузился как файл со второй строкой, а пересохранение писало одну — текст менялся
    // сам собой, та же болезнь, что у `E 007` (решение владельца, третий заход ревью A·2·8).
    // Имя идёт через `quoted` ровно затем же, зачем строка: это байты из файла, и в панель они
    // попадают такими, какими их прислали (ревью аудита #21, A·2·8).
    if (!seen.insert(name).second) {
        if (why) *why = at(where, no, who + "duplicate component " + quoted(name));
        return false;
    }
    const Applied r = apply_component(w, e, name, line.substr(sp + 1));
    if (r == Applied::Ok) return true;
    const char* tail = r == Applied::Unknown  ? ": unknown component"
                       : r == Applied::BadJson ? ": bad json"
                                               : ": value is not what the format writes";
    if (why) *why = at(where, no, who + "component " + quoted(name) + tail);
    return false;
}

} // namespace ide::detail
