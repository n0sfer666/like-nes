// Цель гейта 9: файл сцены IDE. Текстовая, а не двоичная — и потому не лишняя: у её загрузчика
// четырнадцать поименованных причин отказа и разбор JSON внутри значений компонентов, то есть
// ровно та поверхность, где A·2·8 уже находила дефект.
//
// Собирается ТОЛЬКО при `IDE_POC` — `scene_core` без него не существует вовсе. Задача Windows CI
// с clang-cl и ASan собирается с `-DIDE_POC=OFF` намеренно (короткий список целей), поэтому там
// целей на одну меньше, и строка успеха печатает их число, чтобы конфигурация была названа в
// логе, а не угадывалась по коммиту.
#include <cstdint>
#include <string>
#include <vector>

#include "fuzz_target.hpp"
#include "scene.hpp"
#include "serialize.hpp"

namespace fuzz {
namespace {

std::vector<uint8_t> seed_scene() {
    ide::Scene s;
    // Порядок создания не по GUID — тот же приём, что у голден-теста round-trip: сериализация
    // сортирует, и семя обязано это пройти, иначе мутант сравнивался бы с вырожденным случаем.
    auto e10 = s.create(10);
    e10.set<ide::Name>({"hero"});
    e10.set<ide::Position>({fix32::from_int(3), fix32::from_int(5)});
    e10.set<ide::Velocity>({fix32::from_raw(1234), fix32::from_raw(-5678)});

    auto e20 = s.create(20);
    e20.set<ide::Name>({"sword"});
    e20.set<ide::Parent>({10});
    e20.set<ide::Position>({fix32::from_int(1), fix32::from_int(0)});

    auto e5 = s.create(5);
    e5.set<ide::Name>({"camera"});
    e5.set<ide::Position>({fix32::from_float(-2.5), fix32::from_float(7.25)});

    const std::string text = ide::serialize(s);
    return std::vector<uint8_t>(text.begin(), text.end());
}

bool read_scene(const uint8_t* data, size_t size) {
    // flecs печатает СВОЙ разбор JSON в stderr, и на битом вводе это по три строки на случай —
    // сотни тысяч строк за прогон, в которых тонет единственная важная: наша собственная FAIL.
    // Гасится уровень ЧУЖОГО лога, не наш: предмет гейта — падение, а не сообщение.
    static const bool quiet = [] { ecs_log_set_level(-4); return true; }();
    (void)quiet;

    ide::Scene s;
    // `reinterpret_cast` здесь — граница с сырыми байтами мутанта: сцена сериализуется ТЕКСТОМ, а
    // фаззер раздаёт `const uint8_t*`. Доступ к объекту через указатель на char законен, длина
    // передаётся отдельным аргументом, и потому нули внутри буфера строку не обрезают — читатель
    // обязан пережить и их, а не только печатные байты.
    const std::string text(reinterpret_cast<const char*>(data), size);
    std::string why;
    if (!ide::deserialize(s, text, &why)) {
        consume(why.size());
        return false;
    }
    // Обход всего, что загрузчик положил в сцену: голден-хеш проходит по каноническому тексту
    // каждой сущности, то есть ЧИТАЕТ каждый компонент, а не только их число.
    consume(ide::golden_hash(s));
    for (const auto& kv : s.entities()) {
        consume(kv.first);
        consume(ide::serialize_entity(s, kv.first).size());
    }
    return true;
}

const Target TARGETS[] = {
    {"ide-scene", seed_scene, read_scene},
};

} // namespace

const Target* scene_targets(std::size_t* count) {
    *count = sizeof(TARGETS) / sizeof(TARGETS[0]);
    return TARGETS;
}

} // namespace fuzz
