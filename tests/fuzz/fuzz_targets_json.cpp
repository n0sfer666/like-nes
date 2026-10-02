// Цель гейта 9: разборщик JSON пекарей контента (спека #24, В3). Вход — чужой файл из Tiled или
// Aseprite, поэтому разборщик судится тем же гейтом, что читатели секций: мутант не обязан быть
// отвергнут, но обязан не уронить разбор и не вывести дерево за границы входа и арены.
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "byte_arena.hpp"
#include "fuzz_target.hpp"
#include "json.hpp"

namespace fuzz {
namespace {

namespace json = framework::json;

// Семя — ЛИТЕРАЛ, как у манифеста: у JSON нет пекаря в дереве, его пишет чужой редактор. В семени
// собрано всё, у чего в разборщике своя ветка: экранирование и суррогатная пара (арена), сырой
// многобайтный UTF-8, шум Qt и порядок (fix32), край int64, вложенность и пустые контейнеры.
const char* const JSON_SRC =
    "{\"type\": \"map\", \"width\": 4, \"tilewidth\": 16,\n"
    " \"layers\": [{\"name\": \"ground \\u0436\\ud83d\\ude00 \xD0\xB6\xD1\x91\","
    " \"data\": [1, 2, 0, 3],\n"
    "   \"opacity\": 0.5, \"offsetx\": -1.13686837721616e-13,"
    " \"visible\": true, \"properties\": null}],\n"
    " \"tags\": [\"a\\tb\\\"c\\\\d\", \"\", \"x\"], \"big\": -9223372036854775808,"
    " \"exp\": 2.5E+1,\n"
    " \"empty\": {}, \"list\": [[], [[1]]]}\n";

std::vector<uint8_t> seed_json() {
    const std::string s(JSON_SRC);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Обход трогает КАЖДЫЙ байт каждой строки: срез, выведенный за вход или арену, иначе прошёл бы
// мимо санитайзера — длина у него честная, врёт только указатель.
void consume_node(const json::Node& n) {
    consume_all(static_cast<uint8_t>(n.kind), n.first, n.count, n.i, n.f.raw, n.at);
    for (char ch : n.s) consume(static_cast<uint8_t>(ch));
}

bool read_json(const uint8_t* data, size_t size) {
    const auto in = std::as_bytes(std::span(data, size));
    asset::ByteArena arena;
    arena.init(size);
    std::vector<json::Node> doc;
    json::JsonError err;
    if (!json::parse(in, arena, doc, err)) {
        consume(json::format_error("fuzz.json", err).size());
        return false;
    }
    for (const json::Node& n : doc) {
        consume_node(n);
        if (n.kind != json::Kind::Object || n.count == 0) continue;
        const json::Node* v = json::member(doc, n, doc[n.first].s);
        if (v != nullptr) consume_node(*v);
    }
    return true;
}

const Target TARGETS[] = {
    {"json", seed_json, read_json},
};

} // namespace

const Target* json_targets(std::size_t* count) {
    *count = sizeof(TARGETS) / sizeof(TARGETS[0]);
    return TARGETS;
}

} // namespace fuzz
