#include "verify_game.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

#include "bakers.hpp"
#include "bundle_view.hpp"
#include "format.hpp"
#include "hash.hpp"
#include "platform_fs.hpp"

namespace asset {
namespace {

// Секция = логическое имя для отчёта + исходник + пекарь. Добавить сюда tool-free секцию — одна
// строка: guid брать неоткуда, кроме самого пекаря, поэтому имя ассета здесь НЕ дублируется.
struct Section {
    const char* label;
    const char* file;
    bool (*bake)(const std::string&, std::vector<AssetInput>&);
};

const Section SECTIONS[] = {
    {"input presets", "/input.txt", bakers::input_presets},
    {"achievements", "/achievements.txt", bakers::achievements},
    {"movement", "/movement.txt", bakers::movement},
    {"tilemap", "/tilemap.txt", bakers::tilemap},
    {"atlas regions", "/atlas.txt", bakers::atlas_regions},
};

// Расхождение называется по виду, а не одним словом STALE: перепекание снимает разошедшиеся байты,
// но не смену кодека — совет «перепеки» в ответ на неё отправляет читателя чинить не то, а отказ
// после перепекания повторится слово в слово.
enum class Verdict { Match, Codec, Meta, Bytes };

// Сверяется ВСЁ, что заголовок обещает словом «собран из этих исходников», а не только байты:
// `type` и `residency` живут в записи, ими рантайм выбирает путь загрузки секции, и разъехаться
// они могут без единого изменённого байта payload'а. `content_hash` проверяется последним и по
// собственному payload'у бандла — это уже не сверка с исходником, а сверка записи с самой собой.
// Байты сравниваются, а не хеш: у обеих секций кодек `Raw`, то есть payload в бандле и есть
// испечённая таблица; сжатая секция сделала бы сравнение сырых байт бессмысленным.
Verdict compare(const AssetInput& baked, const AssetEntry& e, const uint8_t* payload) {
    if (e.codec != static_cast<uint32_t>(Codec::Raw) ||
        baked.codec != Codec::Raw)
        return Verdict::Codec;
    if (e.type != static_cast<uint32_t>(baked.type)) return Verdict::Meta;
    if (e.residency != static_cast<uint32_t>(baked.residency)) return Verdict::Meta;
    if (e.payload_size != baked.payload.size()) return Verdict::Bytes;
    if (e.uncompressed_size != baked.uncompressed_size) return Verdict::Bytes;
    // Пустая таблица: `data()` пустого вектора вправе быть nullptr, а memcmp требует валидных
    // указателей даже при n == 0. Размеры уже сошлись, сравнивать нечего.
    if (baked.payload.empty()) return Verdict::Match;
    if (std::memcmp(payload, baked.payload.data(), baked.payload.size()) != 0) return Verdict::Bytes;
    if (e.content_hash != fnv1a(baked.payload.data(), baked.payload.size())) return Verdict::Meta;
    return Verdict::Match;
}

// Позитивный контроль: сравнение, которое ничем не опровергнуто, ничего не доказывает. Испорченная
// копия обязана быть отбита ТЕМ ЖЕ кодом, иначе после первой же опечатки в `compare` гейт зеленеет
// вакуумно — ровно то, что правило `vacuous-gate` ловит в шагах CI.
// Портится ТОЛЬКО испечённая сторона, а сравнивается она с настоящим payload'ом бандла: первая
// редакция подсовывала подделку обоими аргументами, memcmp сверял её саму с собой и контроль
// объявлял сломанным исправный `compare`. Ошибка нашлась первым же прогоном — чем контроль и полезен.
// Подделок две, и вердикт у каждой назван точно: «не Match» доказывало бы, что хоть какая-то
// проверка сработала, но не КОТОРАЯ. Байтовая обязана упасть именно на memcmp (Bytes), подмена
// резидентности — именно на метаданных (Meta); выпади любая из двух проверок, контроль это назовёт.
bool rejects_forgeries(const AssetInput& baked, const AssetEntry& e, const uint8_t* payload) {
    AssetInput forged = baked;
    if (forged.payload.empty())
        forged.payload.push_back(0);
    else
        forged.payload[0] = static_cast<uint8_t>(forged.payload[0] ^ 0xff);
    forged.uncompressed_size = static_cast<uint32_t>(forged.payload.size());
    if (compare(forged, e, payload) != Verdict::Bytes) return false;

    AssetInput swapped = baked;
    swapped.residency =
        baked.residency == Residency::Mmap ? Residency::Stream : Residency::Mmap;
    return compare(swapped, e, payload) == Verdict::Meta;
}

} // namespace

bool verify_game_bundle(const std::string& src_dir, const std::string& bundle_path) {
    std::vector<uint8_t> raw;
    if (!platform::read_bytes(bundle_path, raw)) {
        std::fprintf(stderr, "[assetc] verify: cannot read %s\n", bundle_path.c_str());
        return false;
    }
    BundleView view;
    // trusted=false: проверяющему полагается валидированный режим — битая таблица обязана быть
    // названа отказом, а не прочитана мимо границ.
    if (!view.open(raw.data(), raw.size(), false)) {
        std::fprintf(stderr, "[assetc] verify: %s is not a valid bundle\n", bundle_path.c_str());
        return false;
    }

    bool ok = true;
    for (const Section& s : SECTIONS) {
        std::vector<AssetInput> baked;
        if (!s.bake(src_dir + s.file, baked)) {
            ok = false;
            continue;
        }
        if (baked.size() != 1) {
            std::fprintf(stderr, "[assetc] verify: %s baked %zu assets, expected 1\n", s.label,
                         baked.size());
            ok = false;
            continue;
        }
        const AssetEntry* e = view.find(baked[0].guid);
        if (e == nullptr) {
            std::fprintf(stderr, "[assetc] verify: %s MISSING from %s\n", s.label,
                         bundle_path.c_str());
            ok = false;
            continue;
        }
        const Verdict v = compare(baked[0], *e, view.payload(*e));
        if (v == Verdict::Codec) {
            std::fprintf(stderr,
                         "[assetc] verify: %s codec is %u, byte compare only supports Raw (%u).\n"
                         "        A compressed section needs a hash-based check, not this gate.\n",
                         s.label, e->codec, static_cast<uint32_t>(Codec::Raw));
            ok = false;
            continue;
        }
        if (v != Verdict::Match) {
            std::fprintf(stderr,
                         "[assetc] verify: %s STALE (%s) - %s does not match %s%s.\n"
                         "        Rebake: assetc --game %s %s - needs tint and basisu,\n"
                         "        so it runs on the owner's machine only (docs/owner-setup.txt, F).\n",
                         s.label, v == Verdict::Meta ? "metadata" : "payload", bundle_path.c_str(),
                         src_dir.c_str(), s.file, src_dir.c_str(), bundle_path.c_str());
            ok = false;
            continue;
        }
        if (!rejects_forgeries(baked[0], *e, view.payload(*e))) {
            std::fprintf(stderr, "[assetc] verify: %s comparison accepts a corrupted table\n",
                         s.label);
            ok = false;
            continue;
        }
        std::printf("[assetc] verify: %s matches (%u bytes)\n", s.label, e->payload_size);
    }
    // Число сверенных секций печатается всегда: «находок нет» и «нечего было сверять» с одного
    // взгляда неразличимы, а список SECTIONS растёт.
    std::printf("[assetc] verify: %s - %zu tool-free section(s) checked against %s\n",
                ok ? "PASS" : "FAIL", sizeof(SECTIONS) / sizeof(SECTIONS[0]), src_dir.c_str());
    return ok;
}

} // namespace asset
