#include "bundle_view.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "hash.hpp"

namespace asset {

// Проверка конверта отвечает КОДОМ, а не `bool`: какая именно из проверок не сошлась, известно
// ровно здесь, и на возврате `bool` это знание терялось у вызывающего (ревью аудита #21, A·2·5).
// Границу между `WrongVersion` и `Truncated` проводит вопрос «что делать»: первое чинится
// пересборкой бандла, второе — повторной закачкой, и ошибиться советом хуже, чем промолчать.
OpenResult BundleView::envelope_reason(size_t size) const {
    // Подпись спрашивается ПЕРВОЙ, как только под неё есть байты, и лишь потом длина. Иначе совет
    // расходился бы не по причине отказа, а по размеру мусора: присланный не-бандл в двадцать байт
    // получал бы «перекачай», а он же в восемьдесят — «пересобери».
    if (size >= sizeof(MAGIC) && std::memcmp(base_, MAGIC, sizeof(MAGIC)) != 0)
        return OpenResult::WrongVersion;
    // Регион короче заголовка — при целой подписи это ровно недокачанный файл: места под остальные
    // поля в нём нет, спрашивать их не у чего.
    if (size < sizeof(BundleHeader)) return OpenResult::Truncated;
    BundleHeader h;
    std::memcpy(&h, base_, sizeof(h)); // выровненная копия до доверия раскладке
    // Подпись здесь второй раз НЕ спрашивается: до сюда доезжает только регион не короче
    // заголовка, а он заведомо длиннее подписи — то есть повтор был бы правилом, которое нечем
    // сломать, и первым же кандидатом на «этой ветки не бывает» при следующей правке.
    if (h.fmt_version != FMT_VERSION) return OpenResult::WrongVersion; // fail-hard → rebake
    if (h.endian != ENDIAN_LE) return OpenResult::WrongVersion;
    // Размер заголовка — тоже про версию, а не про раскладку этого файла: он равен `sizeof` по
    // построению у КАЖДОГО бандла своей версии, и разойтись может только с другой.
    if (h.header_size != sizeof(BundleHeader)) return OpenResult::WrongVersion;
    if (h.total_size > size) return OpenResult::Truncated;
    // Таблица целиком в границах И выровнена (иначе reinterpret_cast → SIGBUS на strict-align).
    if (h.table_offset % alignof(AssetEntry) != 0) return OpenResult::Malformed;
    uint64_t table_end = static_cast<uint64_t>(h.table_offset) +
                         static_cast<uint64_t>(h.asset_count) * sizeof(AssetEntry);
    if (h.table_offset < sizeof(BundleHeader) || table_end > h.total_size)
        return OpenResult::Malformed;
    // Каждый payload целиком в границах И выровнен (SPIR-V читается как uint32*).
    const AssetEntry* tbl = reinterpret_cast<const AssetEntry*>(base_ + h.table_offset);
    for (uint32_t i = 0; i < h.asset_count; ++i) {
        if (tbl[i].payload_offset % PAYLOAD_ALIGN != 0) return OpenResult::Malformed;
        uint64_t pe = static_cast<uint64_t>(tbl[i].payload_offset) + tbl[i].payload_size;
        if (tbl[i].payload_offset < table_end || pe > h.total_size) return OpenResult::Malformed;
    }
    return OpenResult::Ok;
}

// Штамп конверта против байтов (аудит #21, A·2·5). Поле `bundle_hash` писалось пекарем, печаталось
// `assetc` и было описано в format.hpp как контроль целостности — а читателя у него по дереву не
// было ни одного: обещание формата, которое рантайм не исполнял. Одиночный перевёрнутый байт в
// payload'е (битый диск, оборванная закачка мода) проезжал `open()` целиком — конверт-то цел, — и
// всплывал мусором в геометрии или отказом секционного читателя, где диагностика винила «плохую
// секцию» вместо испорченного файла. Защитой от подделки это не является и не заявлено: FNV не MAC,
// атакующий пересчитает его сам. Ценность ровно диагностическая, и она названа у поля в format.hpp.
//
// Поле обнуляется НЕ в регионе, а в счёте: регион отдан только на чтение (mmap PROT_READ), и восемь
// нулевых байт скармливаются хешу вместо него.
//
// Цена: линейное чтение ВСЕГО заявленного бандла на каждом `open()` — то есть ленивая подкачка
// страниц mmap'а превращается в синхронное чтение. При сегодняшних бандлах (самый большой в дереве
// 136 КБ) это сотни микросекунд и неизмеримо, но вырастет pak — сверку двигать в worker или за
// флаг, иначе модель `Residency::Stream` из шапки заголовка обнуляется этой строкой молча.
//
// Исходов три, а не два, и по той же причине, по какой их семь у `open()`: «заголовок не сошёлся
// сам с собой» чинится пересборкой, «байты не сошлись со штампом» — перекачкой. Один `bool`
// отправлял бы первое вызывающему под именем второго.
OpenResult BundleView::hash_reason() const {
    constexpr size_t AT = offsetof(BundleHeader, bundle_hash);
    constexpr size_t WIDE = sizeof(BundleHeader::bundle_hash);
    const BundleHeader& h = header();
    // Предусловие названо здесь, а не оставлено на вывод из соседней функции, и названы ОБЕ его
    // границы: длина последнего куска считается в size_t, поэтому `total_size` меньше заголовка
    // дал бы не отрицательное число, а чтение на четыре гигабайта мимо региона, а `total_size`
    // больше региона — чтение за его конец ровно на разницу. Сегодня такой конверт отбивает
    // `envelope_reason` раньше, но держать чтение на инварианте ЧУЖОЙ проверки значит потерять его
    // вместе с ней.
    if (h.total_size < AT + WIDE || h.total_size > size_) return OpenResult::Malformed;
    const uint8_t zeros[WIDE] = {};
    uint64_t acc = fnv1a(base_, AT);
    acc = fnv1a(zeros, WIDE, acc);
    acc = fnv1a(base_ + AT + WIDE, h.total_size - AT - WIDE, acc);
    return acc == h.bundle_hash ? OpenResult::Ok : OpenResult::Corrupted;
}

bool BundleView::open(const uint8_t* base, size_t size, bool trusted) {
    base_ = nullptr;
    reason_ = OpenResult::Ok;
    if (!base) {
        reason_ = OpenResult::NoRegion;
        return false;
    }
    // Смещения таблицы и payload'ов сверяются ОТНОСИТЕЛЬНО базы, а сама база до сих пор бралась
    // какой дали: сдвинутый буфер давал `reinterpret_cast` заголовка и таблицы мимо границы — UB, а
    // на strict-align SIGBUS (аудит #21, попутная находка к A·2·6). Проверка стоит и для trusted:
    // цель `header()` читает тем же приведением, а сдвиг — свойство буфера, а не доверия к нему.
    if (reinterpret_cast<std::uintptr_t>(base) % BASE_ALIGN != 0) {
        reason_ = OpenResult::Misaligned;
        return false;
    }
    base_ = base;
    size_ = size;
    if (!trusted) {
        const OpenResult envelope = envelope_reason(size);
        if (envelope != OpenResult::Ok) {
            base_ = nullptr; // reject, НЕ crash
            reason_ = envelope;
            return false;
        }
        // Сверка штампа стоит ПОСЛЕ проверки границ и только после неё: она читает `total_size`
        // байт от базы, и у конверта, который ещё не сошёлся сам с собой, это чтение мимо региона.
        const OpenResult stamp = hash_reason();
        if (stamp != OpenResult::Ok) {
            base_ = nullptr;
            reason_ = stamp;
            return false;
        }
    }
    // trusted всё равно проверяет magic/версию (bit-rot / чужая версия).
    if (trusted) {
        const BundleHeader& h = header();
        if (std::memcmp(h.magic, MAGIC, 4) != 0 || h.fmt_version != FMT_VERSION) {
            base_ = nullptr;
            reason_ = OpenResult::WrongVersion;
            return false;
        }
    }
    return true;
}

} // namespace asset
