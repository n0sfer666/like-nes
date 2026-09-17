#include <chrono>
#include <thread>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "bundle_writer.hpp"
#include "hash.hpp"
#include "platform_args.hpp"

// Headless-гейт #2 (спека #5): zero-copy mmap-резидент + async-стрим→декомпрессия в арену,
// БЕЗ per-frame heap в submit-пути. Гоняется под ASan/UBSan (CI). Не требует GPU.
//
// Вторым предметом здесь стоит КОНВЕРТ бандла: сверка штампа целостности и имя отказа `open()`
// (аудит #21, A·2·5). Место — решение владельца: гейт резидентности и так открывает бандл первым
// делом, а третья цель на `BundleView` (рядом с `asset_align_test`) стоила бы своей записи в CI на
// трёх ОС ради двадцати строк. Предмета два, и шапка называет оба, чтобы файл не описывал молча
// половину себя.

using namespace asset;

namespace {

uint64_t guid_of(const char* n) { return fnv1a(n, std::strlen(n)); }

// Ждём готовности всех запрошенных ассетов через sync_point'ы (детерм. gate тика).
void pump_until_ready(AssetManager& am, const std::vector<uint64_t>& guids, int max_frames) {
    for (int f = 0; f < max_frames; ++f) {
        am.sync_point();
        bool all = true;
        for (uint64_t g : guids) all = all && am.is_ready(g);
        if (all) return;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

int fail(const char* msg) {
    std::fprintf(stderr, "[asset_test] FAIL: %s\n", msg);
    return 1;
}

// Штамп конверта против байтов и имя отказа (аудит #21, A·2·5). Зачем штамп вообще сверяется и
// почему его ценность диагностическая, а не защитная, — сказано у `BundleView::hash_reason`; здесь
// утверждается поведение.
//
// Гейт стоит на бандле, собранном ПИСАТЕЛЕМ, а не на фикстуре из байт: штамп здесь — тот самый,
// который пекарь и кладёт, иначе прогон сверял бы хеш с собственным представлением о раскладке.
// Бандлы остальных наборов приезжают mmap'ом и всегда выровнены постранично, поэтому база тут
// выравнивается руками — по той же причине, что в `asset_align_test`.
int test_a_bent_byte_is_named_corruption() {
    AssetInput a{};
    a.guid = 0x5a5a;
    a.type = AssetType::Raw;
    a.codec = Codec::Raw;
    a.residency = Residency::Mmap;
    a.payload = {9, 8, 7, 6, 5, 4, 3, 2};
    a.uncompressed_size = static_cast<uint32_t>(a.payload.size());
    const std::vector<uint8_t> baked = write_bundle({a});

    constexpr size_t SLACK = 64;
    std::vector<uint8_t> room(baked.size() + BASE_ALIGN + SLACK);
    const auto addr = reinterpret_cast<std::uintptr_t>(room.data());
    uint8_t* base = room.data() + (BASE_ALIGN - addr % BASE_ALIGN) % BASE_ALIGN;
    std::memcpy(base, baked.data(), baked.size());

    BundleView v;
    if (!v.open(base, baked.size(), /*trusted=*/false)) return fail("control: the baked bundle opens");
    const AssetEntry* e = v.find(a.guid);
    if (e == nullptr) return fail("the baked asset is in the table");
    const uint32_t at = e->payload_offset;

    // Чем меряются байты: штамп считается по ЗАЯВЛЕННОМУ бандлу (`total_size`), а не по всему
    // отданному региону. На точном регионе эти два числа совпадают, то есть реализация, считающая
    // по длине региона, прошла бы побайтно так же — а файл, добитый до страницы, это штатный
    // случай mmap, и отказ ему означал бы отказ честному бандлу по чужому хвосту.
    std::memset(base + baked.size(), 0xCD, SLACK);
    if (!v.open(base, baked.size() + SLACK, /*trusted=*/false))
        return fail("control: a region with junk past the end of the bundle still opens");

    // Недокачанный файл назван СВОИМ словом, а не общей «раскладкой»: заголовок в нём цел и честен,
    // байтов за ним меньше, чем он обещает, и совет вызывающему тут «перекачай», а не «пересобери».
    if (v.open(base, baked.size() - 1, /*trusted=*/false)) return fail("a short region is refused");
    if (v.open_reason() != OpenResult::Truncated) return fail("and the refusal is named truncation");

    // И обратная половина той же пары: недокачан — это про ЦЕЛУЮ подпись и нехватку байт за ней, а
    // не про короткий регион как таковой. Спроси читатель длину раньше подписи — один и тот же
    // присланный не-бандл получал бы разный совет в зависимости от размера мусора.
    std::memcpy(base, "NOPE", 4);
    if (v.open(base, 20, /*trusted=*/false)) return fail("a short region that is no bundle is refused");
    if (v.open_reason() != OpenResult::WrongVersion)
        return fail("and a broken signature is named a foreign format, however few bytes came");
    std::memcpy(base, baked.data(), baked.size());

    // Байт ПАЙЛОАДА, а не конверта: подпись, версия, границы и выравнивания после него сходятся
    // ровно как раньше, то есть проверка границ такой файл пропускает по построению — и до сверки
    // штампа его не ловило вообще ничто.
    base[at] ^= 0x01u;
    if (v.open(base, baked.size(), /*trusted=*/false)) return fail("a bent payload byte is refused");
    if (v.open_reason() != OpenResult::Corrupted) return fail("and the refusal is named corruption");
    if (v.valid()) return fail("the rejected view keeps no base");

    // Свой pak открывается mmap'ом каждый старт, и сверка штампа — это чтение ВСЕГО файла: в
    // доверенном режиме её нет, как нет и проверки раскладки (подпись и версию он смотрит всё
    // равно). Это решение о цене, а не забытая ветка, и утверждается оно поведением — иначе
    // завтрашнее «включим везде» проехало бы гейт молча, унеся с собой ленивую подкачку страниц.
    if (!v.open(base, baked.size(), /*trusted=*/true)) return fail("trusted opens the bent bundle");

    // Причины отказа обязаны РАЗЛИЧАТЬСЯ: без этого «файл испорчен» и «чужая версия формата»
    // приезжали бы вызывающему одним словом, а чинятся они разным — перекачкой мода и пересборкой.
    base[at] ^= 0x01u;
    base[0] = 'X';
    if (v.open(base, baked.size(), /*trusted=*/false)) return fail("a foreign envelope is refused");
    if (v.open_reason() != OpenResult::WrongVersion)
        return fail("and it is named a foreign version, not corruption");

    // Контроль наоборот: отказы выше — про ШТАМП и про подпись, а не про то, что виду не нравится
    // любой переписанный буфер. Те же байты обратно — и он их принимает.
    std::memcpy(base, baked.data(), baked.size());
    if (!v.open(base, baked.size(), /*trusted=*/false)) return fail("control: the restored bundle opens");
    // Причина — про ПОСЛЕДНИЙ `open()`, а не про худший из бывших: успех обязан её сбросить.
    // Спрошено ПОСЛЕ отказа и только здесь: сразу после первого открытия тот же вопрос отвечал бы
    // про начальное значение поля, то есть не проверял бы ничего.
    if (v.open_reason() != OpenResult::Ok) return fail("and the reason no longer names the refusal");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 2) return fail("usage: asset_test <bundle> [--selftest]");
    const std::string bundle = argv[1];
    const unsigned delay = (argc >= 3 && std::strcmp(argv[2], "--slow") == 0) ? 3000 : 0;

    if (const int bad = test_a_bent_byte_is_named_corruption()) return bad;

    AssetManager am;
    // validated-режим (bounds-check offset'ов) — строже, чем trusted.
    if (!am.open(bundle, 4u * 1024 * 1024, /*trusted=*/false, delay))
        return fail("open/validate bundle");

    const uint64_t shader_vs = guid_of("sprite.vs");
    const uint64_t albedo = guid_of("hero_albedo");
    const uint64_t bulk = guid_of("scene_bulk");
    std::vector<uint64_t> all = {shader_vs, albedo, bulk, guid_of("sprite.fs"),
                                 guid_of("hero_normal")};

    for (uint64_t g : all) am.request(g);
    am.request(bulk); am.request(albedo); // двойной запрос in-flight — дедуп (high-фикс)
    pump_until_ready(am, all, 500);

    for (uint64_t g : all)
        if (!am.is_ready(g)) return fail("asset not ready after pump");

    // In-flight дедуп (high-фикс): 3 Stream-ассета (bulk+albedo+normal) → ровно 3 арен-аллок,
    // несмотря на двойной request (без дедупа было бы 5). Shader'ы zero-copy — 0 аллок.
    if (am.arena_allocations() != 3) return fail("in-flight dedup: double request re-loaded");

    // (1) Шейдер — zero-copy: указатель ВНУТРИ mmap-региона, content_hash сходится.
    Loaded vs = am.get(shader_vs);
    const AssetEntry* vse = am.view().find(shader_vs);
    if (!vs.zero_copy || vs.data != am.view().payload(*vse)) return fail("shader not zero-copy");
    if (fnv1a(vs.data, vs.size) != vse->content_hash) return fail("shader content_hash");

    // (2) Bulk — zstd декомпрессия в арену корректна (сверка детерм. паттерна).
    Loaded b = am.get(bulk);
    const AssetEntry* be = am.view().find(bulk);
    if (b.zero_copy || b.size != be->uncompressed_size) return fail("bulk size/zero-copy");
    for (uint32_t i = 0; i < b.size; ++i)
        if (b.data[i] != static_cast<uint8_t>((i * 2654435761u) >> 24)) return fail("bulk bytes");

    // (3) Текстура — staged в арену (Phase 3 транскодит BC7).
    Loaded t = am.get(albedo);
    if (t.zero_copy || t.size == 0) return fail("texture staging");

    // (4) Гейт #2: steady-state submit-петля НЕ растит арену (нет per-frame heap-аллокаций).
    uint64_t allocs_before = am.arena_allocations();
    for (int frame = 0; frame < 120; ++frame) {
        am.sync_point();
        volatile const uint8_t* sink = am.get(albedo).data; // «сабмит» без аллокаций
        (void)sink;
    }
    if (am.arena_allocations() != allocs_before) return fail("per-frame arena growth");

    std::printf("[asset_test] PASS zero-copy=shader stream=bulk+tex arena_used=%zu allocs=%llu "
                "delay_us=%u\n",
                am.arena_used(), (unsigned long long)am.arena_allocations(), delay);
    am.close();
    (void)argc;
    return 0;
}
