// Разбор текста сцены: файл целиком и снимок одной сущности для undo. Отделён от записи
// (`serialize.cpp`) не по счётчику строк, а по предмету: запись всегда удаётся, а разбор обязан
// ОТКАЗЫВАТЬ и называть причину. Здесь — ТЕКСТЫ целиком (причины 1–8 из списка в `serialize.hpp`,
// одним местом на весь гейт): порядок строк, шапка, номера сущностей. Разбор ОДНОЙ строки `C` у
// обоих текстов общий и живёт в `parse_component.cpp` — причины 9–14 (A·2·8).
#include "parse_component.hpp"
#include "serialize.hpp"
#include <charconv>
#include <set>
#include <sstream>
#include <system_error>

namespace ide {
namespace {

using detail::apply_line;
using detail::at;
using detail::quoted;

// Один проход по тексту в ЗАДАННУЮ сцену. Отделён от `deserialize` ради того, что проходов два:
// первый — во временную сцену, и только он вправе провалиться, не тронув сцену вызывающего.
bool load_into(Scene& s, const std::string& text, std::string* why) {
    constexpr const char* WHERE = "line ";
    flecs::world& w = s.world();
    std::istringstream in(text);
    std::string line;
    flecs::entity cur;
    std::set<uint64_t> seen;
    std::set<std::string> seen_c;   // имена компонентов ТЕКУЩЕЙ сущности, чистится на каждой `E`
    uint64_t cur_guid = 0;
    bool have_cur = false;
    bool head_seen = false;
    size_t no = 0;
    while (std::getline(in, line)) {
        ++no;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::string bad;
        if (!head_seen) {
            head_seen = true;
            // Шапка обязана быть ПЕРВОЙ непустой строкой и ровно этой: до находки `#` был просто
            // комментарием, и `#!/bin/sh` грузился «успешно» пустой сценой, а первое сохранение
            // затирало документ владельца. Версия — отдельная причина: файл не той породы чинят
            // выбором другого файла, файл будущей версии — другой сборкой.
            if (line == SCENE_HEADER) continue;
            bad = line.starts_with(SCENE_HEADER_PREFIX)
                      ? at(WHERE, no, "unsupported format version: " + quoted(line))
                      : at(WHERE, no, "not a scene file: " + quoted(line));
        } else if (line[0] == '#') {
            // Комментарий после шапки и пустая строка ПРИНИМАЮТСЯ и пропадают на первом же
            // пересохранении — осознанно: `serialize` пишет только `E` и `C`. Теряется при этом
            // ОФОРМЛЕНИЕ, а не данные: сцена после круга та же, и отказывать за строку, которой
            // формат не запрещает, значило бы отнять единственный способ подписать сейв, правленный
            // руками. В тот же класс попадают пустые строки ДО шапки и пробелы внутри значения:
            // принимаются, теряются, данные те же. Причина 4 (ведущий ноль) стоит НЕ на этом
            // доводе, а на своём: там круг меняет НОМЕР сущности, то есть данные (решение
            // владельца по ревью аудита #21, A·2·8).
            continue;
        } else if (line.starts_with("E ")) {
            // Цифрой обязана быть КАЖДАЯ, а не первая: разбор на `20abc` останавливался на букве
            // и отдавал 20 — сущность рождалась под номером, которого в файле нет, а хвост
            // исчезал без следа.
            //
            // Не влезшие цифры — СВОЯ причина, а не «не число» (решение владельца, как и у `api=`
            // в A·2·7): `strtoull` насыщался до `ULLONG_MAX`, выставлял `ERANGE`, которого никто
            // не читал, и первое же сохранение записывало `E 18446744073709551615` вместо
            // исходной строки. Опечатка чинится правкой символа, переполнение — другим номером.
            //
            // Ведущий ноль — СВОЯ причина, а не «не число» (решение владельца): все символы тут
            // цифры, и чинится он не правкой символа, а стиранием нуля. `E 007` разбирался в 7, и
            // пересохранение писало `E 7` — два текста одного номера, то есть тот же дубль, что у
            // причины 6, только невидимый глазу.
            const std::string id = line.substr(2);
            bool digits = !id.empty();
            for (char c : id) if (c < '0' || c > '9') digits = false;
            if (!digits) bad = at(WHERE, no, "entity id is not a number: " + quoted(line));
            else if (id.size() > 1 && id[0] == '0')
                bad = at(WHERE, no, "entity id has a leading zero: " + quoted(line));
            else if (std::from_chars(id.data(), id.data() + id.size(), cur_guid).ec != std::errc{})
                bad = at(WHERE, no, "entity id does not fit: " + quoted(line));
            // Дубль номера УНИЧТОЖАЛ первый блок молча: `Scene::create` зовёт `destruct()` на
            // существующей сущности, и компоненты из первой половины файла исчезали без слова —
            // то есть ровно «полусцена, которая выглядит целой», предмет этой находки.
            else if (!seen.insert(cur_guid).second)
                bad = at(WHERE, no, "duplicate entity id: " + quoted(line));
            else {
                cur = s.create(cur_guid);
                seen_c.clear();
                have_cur = true;
            }
        } else if (!line.starts_with("C ")) {
            // Строка, которой в формате нет вовсе, — тоже отказ (решение владельца): иначе
            // `E20` без пробела молча терял бы сущность вместе со всеми её компонентами.
            bad = at(WHERE, no, "unrecognized line: " + quoted(line));
        } else if (!have_cur) {
            bad = at(WHERE, no, "component before any entity: " + quoted(line));
        } else {
            apply_line(w, cur, cur_guid, line, WHERE, no, seen_c, &bad);
        }
        if (!bad.empty()) {
            if (why) *why = bad;
            return false;
        }
    }
    // Пустой файл шапки не объявил, а значит и сценой себя не назвал. Номер строки тут 1, а не
    // `no`: искать нечего, чинится такой файл добавлением первой строки.
    if (!head_seen) {
        if (why) *why = at(WHERE, 1, std::string("not a scene file: ") + quoted(""));
        return false;
    }
    return true;
}

// Один проход ТЕЛА снимка в заданную сцену — как `load_into` у файла и ровно затем же: проходов
// два. Диспетчер строк у снимка свой: шапки и строк `E` у него нет, он — тело одной сущности.
bool load_body(Scene& s, uint64_t guid, const std::string& body, std::string* why) {
    // Свой префикс: строка снимка и строка файла сцены — разные тексты, и номер, поданный под
    // одним именем, увёл бы искать правку в произвольное место сейва (ревью аудита #21, A·2·8).
    constexpr const char* WHERE = "snapshot line ";
    flecs::entity e = s.create(guid);
    flecs::world& w = s.world();
    std::istringstream in(body);
    std::string line;
    std::set<std::string> seen_c;
    size_t no = 0;
    while (std::getline(in, line)) {
        ++no;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::string bad;
        if (!line.starts_with("C ")) bad = at(WHERE, no, "unrecognized line: " + quoted(line));
        else apply_line(w, e, guid, line, WHERE, no, seen_c, &bad);
        if (!bad.empty()) {
            if (why) *why = bad;
            return false;
        }
    }
    return true;
}

} // namespace

// Отказ — ВСЕЙ сцены, а не строки (решение владельца). Сцена, собранная из части файла, выглядит
// целой: половина сущностей на месте, у второй половины позиция (0,0) — то есть редактор показал бы
// не «файл битый», а «кто-то передвинул объекты в начало координат», и первое же сохранение
// закрепило бы потерю. Диагностика называет строку, сущность и компонент: три координаты, по
// которым правку ищут в файле (аудит #21, A·2·8).
bool deserialize(Scene& s, const std::string& text, std::string* why) {
    if (why) why->clear();
    // Проход первый — во ВРЕМЕННУЮ сцену: отказ обязан оставить сцену вызывающего нетронутой.
    // Прежний `s.clear()` стоял ДО первой проверки, и чужой файл, выбранный по ошибке, убивал уже
    // открытый документ — то есть та же потеря, против которой заведена находка, только целиком
    // (ревью аудита #21, A·2·8). Разбор детерминирован, поэтому второй проход не может отказать;
    // очистка на его отказе стоит здесь не ради случая, а ради инварианта «пусто или целое».
    // Область видимости у пробы своя, и это не косметика: сцена на 10k сущностей — это целый мир
    // flecs, и проба, дожившая до конца функции, держала бы его вторым, пока идёт второй проход.
    // Пик памяти на открытии файла удваивался ровно на ту сцену, которую уже приняли (ревью, A·2·8).
    { Scene probe;
      if (!load_into(probe, text, why)) return false; }
    s.clear();
    if (load_into(s, text, why)) return true;
    s.clear();
    return false;
}

// Тот же отказ на пути undo (решение владельца): снимок пишет `serialize_entity`, а прочитать его
// может и чужая сборка — тело строки `C` общее с файлом сцены. Сцена при отказе НЕ ТРОНУТА, тем же
// двойным проходом, что у `deserialize`: прежде разбор шёл прямо в неё, и битое тело оставляло на
// месте занятого guid полусобранную сущность — та выглядит целой, а история уже считает шаг
// отменённым. Отказ второго прохода недостижим (разбор детерминирован, а первый прошёл) и стоит
// здесь ради ИНВАРИАНТА «цело или не тронуто», а не ради случая (ревью аудита #21, A·2·8).
bool restore_entity(Scene& s, uint64_t guid, const std::string& body, std::string* why) {
    if (why) why->clear();
    { Scene probe;   // та же забота о пике памяти, что у `deserialize`
      if (!load_body(probe, guid, body, why)) return false; }
    if (load_body(s, guid, body, why)) return true;
    s.destroy(guid);
    return false;
}

} // namespace ide
