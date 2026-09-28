#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "asset_manager.hpp"
#include "hash.hpp"
#include "platform_args.hpp"
#include "transcode.hpp"

#include "basisu_transcoder.h"

// Гейт потолка транскодера KTX2 (аудит #21, A·2·9). Число блоков уровня приходит из заголовка
// чужого файла, и `bc7.resize` шёл по нему без единой НАШЕЙ проверки — всё, что стояло между
// файлом и выделением памяти, было внутренней валидацией basisu, исходников которой в дереве нет.
//
// Гейт стоит на трёх ногах, и ни одна не лишняя:
//   1. таблица входов на чистой функции `level_blocks` — что именно решено и на каких границах;
//   2. ПОЛОЖИТЕЛЬНЫЙ контроль на настоящем атласе из бандла — иначе таблица осталась бы верна и у
//      реализации, отвергающей всё подряд;
//   3. ВРАЖДЕБНЫЙ файл — иначе не проверен сам вызов: подмена `level_blocks(...)` на
//      `info.m_total_blocks` собирается молча, и первые две ноги её не видят.
//
// Возражение против третьей ноги было такое: файл отвергается ГДЕ-ТО, и отличить наш отказ от
// внутреннего отказа basisu по возврату `bool` нечем. Оно снято не словами, а порядком контролей:
// фикстура сперва обязана ПРОЙТИ все три шага basisu (`init`, `start_transcoding`,
// `get_image_level_info`) с подменёнными сторонами, и только потом упирается в наш потолок. Если
// чужой код принял файл, а `ktx2_to_bc7` его отверг, отвергли его мы — больше некому.
using namespace asset;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "[transcode_test] FAIL: %s\n", what);
    ++failures;
}

int fail(const char* msg) {
    std::fprintf(stderr, "[transcode_test] FAIL: %s\n", msg);
    return 1;
}

constexpr uint32_t HOSTILE_SIDE = 16384;

uint64_t guid_of(const char* n) { return fnv1a(n, std::strlen(n)); }

bool wait_ready(AssetManager& am, uint64_t guid) {
    for (int f = 0; f < 500; ++f) {
        am.sync_point();
        if (am.is_ready(guid)) return true;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    return false;
}

// Смещения сторон в шапке KTX2: 12 байт опознавателя, затем vkFormat, typeSize — и сразу
// pixelWidth/pixelHeight. Меняем именно их, потому что `m_total_blocks` в файле не лежит: basisu
// СЧИТАЕТ его по сторонам, и подменять надо причину, а не следствие.
constexpr size_t KTX2_PIXEL_WIDTH_OFF = 20;
constexpr size_t KTX2_PIXEL_HEIGHT_OFF = 24;

// Единственный сторож длины во всей подмене: гейт про «не индексируй буфер непроверенным числом»
// обязан начинать с себя, а дублировать его у зовущего было бы кодом, который нечем покрасить.
// Возврат — гигиена самого хелпера, а не контроль гейта, поэтому зовущий на нём обрывает прогон,
// а не засчитывает утверждение: утверждение, истинное по построению, не покрасит ни одна мутация.
bool poke_u32(std::vector<uint8_t>& v, size_t off, uint32_t value) {
    if (v.size() < off + sizeof value) return false;
    std::memcpy(v.data() + off, &value, sizeof value);
    return true;
}

// Число блоков, которое уровень занимает ЧЕСТНО. Считается здесь второй раз и нарочно другим
// выражением, чем в `level_blocks`: оракул, списанный с проверяемого кода, подтвердил бы любую
// ошибку округления вместе с ним.
uint64_t honest(uint32_t w, uint32_t h) {
    uint64_t bw = 0, bh = 0;
    for (uint64_t x = 0; x < w; x += 4) ++bw;
    for (uint64_t y = 0; y < h; y += 4) ++bh;
    return bw * bh;
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 2) return fail("usage: transcode_test <bundle>");

    // Нулевой стороны в таблице нет, и это не пропуск: она даёт ноль блоков, то есть тот же ноль,
    // которым тут называется отказ. Утверждать это значило бы завести строку, которую не покрасит
    // ни одна мутация (см. `transcode.hpp`).
    //
    // Потолок держится по БЛОКАМ. Две строки ниже — квадратные уровни по разные стороны от него:
    // у 65535×65535 блоков 268 млн (в 64 раза над потолком), у 8192×8192 — ровно 1<<22. Именно
    // КВАДРАТНЫЙ: столько же блоков стоит и несимметричный 16384×4096 четырьмя строками ниже, так
    // что «самого большого» уровня вообще не существует — есть самый большой при равных сторонах.
    check(level_blocks(65535, 65535, honest(65535, 65535)) == 0,
          "a level past the block ceiling is refused even though its block count is honest");
    check(level_blocks(8192, 8192, honest(8192, 8192)) == honest(8192, 8192),
          "and the largest square level the ceiling does allow is taken");
    // Ровно на границе и ровно за ней: потолок, поставленный не туда, виден только здесь.
    check(level_blocks(1 << 13, (1 << 13) + 4, honest(1 << 13, (1 << 13) + 4)) == 0,
          "one row of blocks past the ceiling is already past it");
    // ТОНКИЙ уровень — строка, которой у таблицы не было, и потому первая редакция потолка мерила
    // ПЛОЩАДЬ: 1×(1<<26) — ровно 1<<26 текселей, предел по площади он проходит, а блоков даёт
    // 16 777 216, то есть 256 МиБ вместо обещанных 64. Округление вверх идёт по КАЖДОЙ стороне,
    // поэтому площадь число байт не пинит, а число блоков пинит.
    check(level_blocks(1, 1u << 26, honest(1, 1u << 26)) == 0,
          "a thin level within the texel count but past the block count is refused");
    // И зеркало к ней: несимметричный уровень РОВНО на потолке. 16384×4096 — те же 1<<22 блока,
    // что и 8192×8192, то есть ровно те же 64 МиБ. Потолок по СТОРОНЕ 8192 отверг бы его, не
    // спросив цену; без этой строки такую подмену не красит ничего.
    check(level_blocks(16384, 4096, honest(16384, 4096)) == honest(16384, 4096),
          "a lopsided level costing exactly the ceiling is taken, not judged by its longest side");

    // Сверка `declared == need` стоит отдельно от потолка, и сторожит она не файл, а ШОВ с basisu:
    // числа блоков в KTX2 нет, basisu считает его из тех же сторон, так что файл им соврать не
    // может. Разойтись наши округления способны при смене версии транскодера или стороны блока —
    // и тогда буфер окажется не того размера. Две строки ниже — обе стороны расхождения.
    check(level_blocks(64, 64, 4096) == 0, "a block count above what the sides need is refused");
    check(level_blocks(64, 64, 255) == 0, "and a block count short of the sides is refused too");
    check(level_blocks(64, 64, 256) == 256, "control: a level that adds up is taken");

    // Сторона, не кратная четырём, занимает неполный блок ЦЕЛИКОМ. Пол вместо потолка дал бы
    // буфер короче того, во что транскодер пишет, — то есть запись за конец `bc7`.
    check(level_blocks(63, 63, 256) == 256, "a side that is not a multiple of four rounds up");
    check(level_blocks(1, 1, 1) == 1, "and a level of a single pixel is one whole block");

    // Положительный контроль на НАСТОЯЩЕМ файле: атлас из бандла, испечённый нашим `assetc`,
    // обязан пройти потолок и дать ровно столько байт, сколько занимают его блоки. Без этого
    // утверждения таблица выше осталась бы верна и у `level_blocks`, возвращающей ноль всегда.
    AssetManager am;
    if (!am.open(argv[1], 8u * 1024 * 1024, /*trusted=*/false)) return fail("open bundle");
    const uint64_t g_px = guid_of("atlas");
    am.request(g_px);
    if (!wait_ready(am, g_px)) { am.close(); return fail("atlas asset never became ready"); }

    const Loaded a = am.get(g_px);
    std::vector<uint8_t> bc7;
    uint32_t w = 0, h = 0;
    check(ktx2_to_bc7(a.data, a.size, bc7, w, h), "control: the baked atlas transcodes");
    check(w > 0 && h > 0, "control: and it reports its size");
    check(bc7.size() == static_cast<size_t>(honest(w, h)) * 16,
          "control: and the bytes it produced are exactly its blocks");

    // Тот же файл с подменёнными сторонами. 16384×16384 — вчетверо над потолком блоков; заявлены
    // они при этом ЧЕСТНО (basisu пересчитал их по новым сторонам), так что сверка тут молчит и
    // отвергает именно потолок. Цена пропуска — `resize` на 256 МБ по восьми байтам чужой шапки;
    // на 65535×65535, которые basisu принимает точно так же, это уже 4 ГБ.
    // Отдельного сторожа на длину ассета тут НЕТ, и это не пропуск: её сверяет сам `poke_u32`, а
    // ветку «payload пуст» нечем покрасить — `wait_ready` выше уже дождался готовности, и строкой
    // раньше тот же указатель ушёл в `ktx2_to_bc7`. Тот же критерий, по которому из `level_blocks`
    // выброшена ветка нулевой стороны: ни одна мутация не отличила бы её наличия от отсутствия.
    std::vector<uint8_t> evil(a.data, a.data + a.size);
    if (!poke_u32(evil, KTX2_PIXEL_WIDTH_OFF, HOSTILE_SIDE) ||
        !poke_u32(evil, KTX2_PIXEL_HEIGHT_OFF, HOSTILE_SIDE)) {
        am.close();
        return fail("patching the hostile fixture");
    }
    const uint32_t evil_size = static_cast<uint32_t>(evil.size());

    // Контроль отличимости: всё, что проверяет basisu, фикстура проходит. Первый из четырёх
    // ОБРЫВАЕТ прогон, а не засчитывает утверждение: `init` выставляет `m_pData` раньше проверок
    // сторон и формата, поэтому у не разобравшегося транскодера сторож `if (!m_pData)` внутри
    // `start_transcoding` промолчит, и дальше контроль опирался бы на чужих сторожей в
    // зависимости, которая тянется FetchContent'ом. Сценарий «новая версия basisu перестала
    // принимать подменённую шапку» — ровно тот, ради которого эти контроли и написаны.
    basist::ktx2_transcoder probe;
    if (!probe.init(evil.data(), evil_size)) {
        am.close();
        return fail("control: basisu itself accepts the patched header");
    }
    check(probe.start_transcoding(), "control: and starts transcoding the patched file");
    basist::ktx2_image_level_info li{};
    check(probe.get_image_level_info(li, 0, 0, 0), "control: and reports a level for it");
    check(li.m_total_blocks == honest(HOSTILE_SIDE, HOSTILE_SIDE),
          "control: whose block count follows the patched sides, so the tally has nothing to catch");

    // Значит отказ ниже может быть только НАШ. Но САМ отказ уликой не служит: basisu на этом файле
    // тоже вернёт false — просто уже после того, как под заявленные блоки выделено 256 МБ. Улика в
    // том, что буфера НЕТ: пустой `evil_bc7` означает, что до `resize` дело не дошло, то есть
    // потолок сработал там, где обязан, — между чужим числом и выделением памяти.
    std::vector<uint8_t> evil_bc7;
    uint32_t ew = 0, eh = 0;
    const bool taken = ktx2_to_bc7(evil.data(), evil_size, evil_bc7, ew, eh);
    check(!taken && evil_bc7.empty(),
          "a header claiming 16384x16384 is refused before it buys a single byte");
    am.close();

    const bool pass = failures == 0;
    std::printf("[transcode_test] %s atlas=%ux%u bc7=%zu ceiling=%llu blocks\n",
                pass ? "PASS" : "FAIL", w, h, bc7.size(),
                static_cast<unsigned long long>(MAX_TEXTURE_BLOCKS));
    return pass ? 0 : 1;
}
