#pragma once

#include <string>
#include <vector>

#include "platform_fs.hpp"
#include "platformer_input_wire.hpp"
#include "stream.hpp"

// Записанный прогон в файл и обратно (спека #22, гейт 6). Едет ровно то, что верификатор
// переигрывает: ввод построчно и заявка о состоянии на каждый тик. Сцена не едет вовсе — она и есть
// то, что реплей обязан ВОССТАНОВИТЬ, а не получить готовой.
//
// Строка ввода пишется тем же `input_wire`, что и датаграмма пира: у формата один автор, и второй
// написал бы набивку `MoveInput` в файл ровно так, как первый научился её не писать.
//
// Номер тика в строке ИЗБЫТОЧЕН — он выводится из порядка — и лежит там намеренно: перестановка
// строк местами не меняет ни длины файла, ни его хвоста, и без этого поля читалась бы как честный
// поток, разошедшийся на первом же переставленном тике.
namespace platformer::replay_io {

using Stream = framework::replay::Stream<ch::MoveInput>;

// Заголовок: подпись, число игроков, число тиков.
constexpr size_t HEAD = 4 + 4 + 4;
constexpr size_t CLAIM = 8;
constexpr uint8_t MAGIC[4] = {'R', 'P', 'L', '1'};

inline uint64_t row_bytes(uint64_t players) { return players * input_wire::BYTES + CLAIM; }

// Длина СВОЕГО потока: числа здесь наши и малы по построению (ширина сессии, длина скрипта).
inline uint64_t bytes_for(uint64_t players, uint64_t ticks) {
    return HEAD + ticks * row_bytes(players);
}

// Длина ЧУЖОГО файла — тот же вопрос, заданный делением, и разница не стилистическая. Умножение
// заявленных чисел ПЕРЕПОЛНЯЕТСЯ: `players = 954437176`, `ticks = 2^31` дают в uint64 ровно 12, то
// есть файл в один заголовок сходится сам с собой, проходит проверку и заставляет выписать под
// строку 7.6 ГБ — ровно ту аллокацию, ради запрета которой проверка и стоит. Деление не
// переполняется никогда: `row` ограничен шириной `players` (uint32 · 9 + 8), а остаток и частное
// от РЕАЛЬНОГО размера файла заведомо не больше него самого.
inline bool size_matches(uint32_t players, uint32_t ticks, size_t size) {
    if (players == 0 || size < HEAD) return false;
    const uint64_t body = static_cast<uint64_t>(size) - HEAD;
    const uint64_t row = row_bytes(players);
    if (ticks == 0) return body == 0;
    return body % row == 0 && body / row == ticks;
}

inline bool write_file(const std::string& path, const Stream& s) {
    if (s.players() == 0) return false;
    std::vector<uint8_t> buf(static_cast<size_t>(bytes_for(s.players(), s.ticks())));
    net::Writer w(buf.data(), buf.size());
    w.bytes(MAGIC, sizeof(MAGIC));
    w.u32(s.players());
    w.u32(s.ticks());
    for (framework::replay::Tick t = 0; t < s.ticks(); ++t) {
        const ch::MoveInput* row = s.row(t);
        for (uint32_t p = 0; p < s.players(); ++p) {
            uint8_t body[input_wire::BYTES];
            if (!input_wire::put(body, t, row[p])) return false;
            w.bytes(body, sizeof(body));
        }
        w.u64(s.claim(t));
    }
    if (!w.ok() || w.size() != buf.size()) return false;
    std::FILE* f = platform::open_file(path, "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(buf.data(), 1, buf.size(), f) == buf.size();
    return std::fclose(f) == 0 && ok;
}

// Длина сверяется с ЗАГОЛОВКОМ до чтения строк, то есть перед первым `reset`: «недоверенный файл»
// здесь означает ровно то же, что «недоверенная датаграмма» в `net_wire` — размер берётся у ТОГО,
// ЧТО ПРИШЛО, а не у того, что пришедшее сообщает о себе. Сочинённый заголовок иначе заставил бы
// выписать под строку память и только потом получил бы отказ.
//
// Отсюда же следует, каких файлов короткие ветки читателя ниже НЕ ВИДЯТ: обрез, дописанный хвост и
// полтика — всё это расхождение длины, и валит их одна строка выше. До чтения доезжает только файл
// верной длины с испорченным содержимым: переставленные строки, чужой номер тика внутри строки.
inline bool parse(const std::vector<uint8_t>& bytes, Stream& s) {
    net::Reader r(bytes.data(), bytes.size());
    uint8_t magic[4] = {};
    uint32_t players = 0;
    uint32_t ticks = 0;
    if (!r.bytes(magic, sizeof(magic)) || !r.u32(&players) || !r.u32(&ticks)) return false;
    for (size_t i = 0; i < sizeof(magic); ++i)
        if (magic[i] != MAGIC[i]) return false;
    if (!size_matches(players, ticks, bytes.size())) return false;

    s.reset(players);
    std::vector<ch::MoveInput> row(players);
    for (uint32_t t = 0; t < ticks; ++t) {
        for (uint32_t p = 0; p < players; ++p) {
            uint8_t body[input_wire::BYTES];
            uint32_t at = 0;
            if (!r.bytes(body, sizeof(body))) return false;
            if (!input_wire::get(body, sizeof(body), at, row[p]) || at != t) return false;
        }
        uint64_t claim = 0;
        if (!r.u64(&claim)) return false;
        if (!s.record(row.data(), claim)) return false;
    }
    // Про остаток читатель не спрашивает, и это не забывчивость: записи фиксированной длины, и
    // равенство размера заголовку УЖЕ означает, что за последним тиком байтов нет. Второй вопрос о
    // том же был бы правилом, которое нечем сломать, — а такое неотличимо от отсутствующего.
    return r.ok();
}

// Разбор отделён от чтения файла, потому что у потока два входа: свой файл и байты, забранные у
// ЧУЖОГО процесса (шаг C). Второй вход через временный файл значил бы, что гейт двух процессов
// проверяет заодно и файловую систему, а подделку правил бы не в том, что приехало, а в своей копии.
inline bool read_file(const std::string& path, Stream& s) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes(path, bytes)) return false;
    return parse(bytes, s);
}

} // namespace platformer::replay_io
