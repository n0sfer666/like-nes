// Цели гейта 9 из достижений: манифест (LNAM) и снимок состояния (LNAP). Отдельной единицей от
// `fuzz_targets_engine.cpp` по тому же правилу, по которому разделены остальные группы, — ПО
// ЗАВИСИМОСТЯМ: здесь линкуется `ach_core`, а там `asset_core` и `material_*`. Повод к разделению
// дал бюджет длины, но граница выбрана не по числу строк: у этой пары своя библиотека и свой
// формат, и второй цели из неё (снимку) нужен шов перепечатки, которого остальным не нужно.
// `asset/hash.hpp` ниже заголовочный: кода он не линкует и границу групп не двигает.
#include <vector>

// Пути КАТАЛОГОМ, а не одним именем: в дереве по два `bake.hpp`, `manifest.hpp`, `registry.hpp` и
// `state.hpp` — у материалов и достижений, у достижений и плагинов, у достижений и персонажа.
#include "achievements/bake.hpp"
#include "achievements/manifest.hpp"
#include "achievements/registry.hpp"
#include "achievements/state.hpp"
#include "asset/hash.hpp"
#include "fuzz_target.hpp"

namespace fuzz {
namespace {

// --- манифест достижений (LNAM) ---

const char* const ACH_SRC =
    "stat | stat_kills\n"
    "stat | stat_score\n"
    "ach | FIRST_BLOOD | progress | stat_kills | 1   | -      | First Blood | Kill one enemy\n"
    "ach | SCORE_500   | progress | stat_score | 500 | -      | High Scorer | Reach 500 points\n"
    "ach | NO_DAMAGE   | bool     |            |     | hidden | Untouchable | Take no damage\n";

std::vector<uint8_t> seed_ach_manifest() {
    std::vector<uint8_t> bytes;
    ach::BakeError err;
    ach::bake_manifest(ACH_SRC, bytes, err);
    return bytes;
}

bool read_ach_manifest(const uint8_t* data, size_t size) {
    ach::Registry reg;
    if (ach::load_manifest(reg, data, size) != ach::LoadResult::Ok) return false;
    for (const ach::Entry& e : reg.entries()) {
        consume_all(e.def.id, e.def.stat, e.def.target, e.def.kind, e.def.flags);
        consume_str(e.key);
        consume_str(e.name);
        consume_str(e.desc);
        consume(reg.find(e.def.id) != nullptr ? 1u : 0u);
    }
    for (const ach::Stat& s : reg.stats()) {
        consume(s.id);
        consume_str(s.key);
        consume(reg.stat_index(s.id));
    }
    return true;
}

// --- снимок состояния достижений (LNAP) ---

std::vector<uint8_t> seed_ach_state() {
    ach::Snapshot snap;
    snap.stats = {{0x1111ull, 3}, {0x2222ull, 500}};
    snap.unlocked = {0x3333ull, 0x4444ull};
    std::vector<uint8_t> bytes;
    ach::encode(snap, bytes);
    return bytes;
}

bool read_ach_state(const uint8_t* data, size_t size) {
    ach::Snapshot out;
    if (ach::decode(data, size, out) != ach::DecodeResult::Ok) return false;
    for (const ach::StatRecord& s : out.stats) consume_all(s.id, s.value);
    for (ach::Id id : out.unlocked) consume(id);
    return true;
}

// Снимок запечатан хешем FNV по всему, что лежит за заголовком (`ach::decode`), а размер сверен со
// счётчиками. Поэтому мутация payload'а отбивается хешем, мутация счётчиков — размером, и без
// перепечатки внутрь не проходит НИЧЕГО: за 16000 случаев цель не приняла ни одного изменённого
// буфера, то есть цикл записей ни разу не исполнился на враждебных байтах. Перепечатка возвращает
// содержимое под обстрел, не трогая ни магию, ни версию, ни счётчики: их мутации отбивает тот же
// конверт и теми же проверками, что до неё.
void reseal_ach_state(std::vector<uint8_t>& buf) {
    // Короче заголовка — чинить нечего: такой вход обязан отбиваться по длине, а не по хешу.
    if (buf.size() < ach::STATE_HEADER_SIZE) return;
    const uint64_t h =
        asset::fnv1a(buf.data() + ach::STATE_HEADER_SIZE, buf.size() - ach::STATE_HEADER_SIZE);
    // Смещение — ТО ЖЕ имя, которым пишет `ach::encode` и читает `ach::decode`. Своя запись числа
    // `16` тут была бы третьей копией: разойдись она с писателем, перепечатка тихо портила бы
    // соседнее поле, а гейт сообщал бы о вакууме вместо настоящей причины.
    for (std::size_t i = 0; i < ach::STATE_HASH_SIZE; ++i) {
        buf[ach::STATE_HASH_OFFSET + i] = static_cast<uint8_t>(h >> (i * 8));
    }
}

const Target TARGETS[] = {
    {"ach-manifest", seed_ach_manifest, read_ach_manifest},
    {"ach-state", seed_ach_state, read_ach_state, reseal_ach_state},
};

} // namespace

const Target* ach_targets(std::size_t* count) {
    *count = sizeof(TARGETS) / sizeof(TARGETS[0]);
    return TARGETS;
}

} // namespace fuzz
