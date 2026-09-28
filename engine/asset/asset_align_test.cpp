#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "bundle_view.hpp"
#include "bundle_writer.hpp"
#include "platform_args.hpp"

// Выравнивание БАЗЫ бандла (аудит #21, попутная находка к A·2·6). Смещения таблицы и payload'ов
// `BundleView` сверяет относительно базы, а саму базу принимает какой дали: сдвинутый на байт буфер
// даёт приведение заголовка и таблицы мимо границы — UB, на strict-align SIGBUS. Гейт отдельной
// целью, потому что остальные наборы ассетов открывают бандл через mmap, а он всегда постранично
// выровнен: дефект не виден ни одному из них по построению.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace asset;

    // Бандл собран писателем, а не руками: фикстура, выписанная байтами, сверялась бы с
    // собственным представлением о раскладке, а предмет здесь — адрес, по которому её читают.
    AssetInput a{};
    a.guid = 0x1234;
    a.type = AssetType::Raw;
    a.codec = Codec::Raw;
    a.residency = Residency::Mmap;
    a.payload = {1, 2, 3, 4};
    a.uncompressed_size = 4;
    const std::vector<uint8_t> bundle = write_bundle({a});

    // Требование к базе сверяется с требованиями ФОРМАТА, а не берётся у самой константы: шаги
    // цикла ниже считаются по `BASE_ALIGN`, и ослабь её кто-нибудь до `alignof(BundleHeader)` —
    // прогон согласился бы с ней молча, проверяя ровно ту границу, которую и ослабили.
    check(BASE_ALIGN % PAYLOAD_ALIGN == 0, "the base requirement covers the payload alignment");
    check(BASE_ALIGN % alignof(BundleHeader) == 0, "the base requirement covers the header");
    check(BASE_ALIGN % alignof(AssetEntry) == 0, "the base requirement covers a table entry");

    // Точка отсчёта выравнивается здесь же, а не берётся у аллокатора на веру: иначе «сдвиг 0» был
    // бы выровнен так, как повезло куче, и половина прогона молча проверяла бы не тот адрес.
    std::vector<uint8_t> room(bundle.size() + 2 * BASE_ALIGN);
    const auto addr = reinterpret_cast<std::uintptr_t>(room.data());
    uint8_t* origin = room.data() + (BASE_ALIGN - addr % BASE_ALIGN) % BASE_ALIGN;

    // Сдвиг проходится ЦЕЛИКОМ, а не одним нечётным байтом: требование базы — 16 (payload читается
    // как uint32* и как SIMD), и сдвиг на 8 оставался бы выровненным по заголовку, но не по
    // payload'у. Один нечётный сдвиг такую границу не отличает (аудит #21, ревью A·2).
    BundleView v;
    for (std::size_t shift = 0; shift <= BASE_ALIGN; ++shift) {
        std::memcpy(origin + shift, bundle.data(), bundle.size());
        const bool expected = shift % BASE_ALIGN == 0;
        for (int trusted = 0; trusted < 2; ++trusted)
            if (v.open(origin + shift, bundle.size(), trusted != 0) != expected) {
                std::printf("  FAIL: the bundle at shift %u %s (trusted=%d)\n",
                            static_cast<unsigned>(shift), expected ? "was refused" : "opened",
                            trusted);
                ++fails;
            }
    }
    // Отказ обязан ещё и НЕ оставить базы: вид, сохранивший её, дальше читает по чужому адресу.
    // Прогон выше кончается выровненным сдвигом, поэтому отказ здесь ставится отдельно.
    v.open(origin + 1, bundle.size(), /*trusted=*/false);
    check(!v.valid(), "the rejected view keeps no base");

    const bool pass = (fails == 0);
    std::printf("asset-align: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
