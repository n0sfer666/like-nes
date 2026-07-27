#include "platform_shmem.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

#include "platform_guard.hpp"
#include "platform_process.hpp"

// Шов platform_shmem (спека #13, гейт 1) на трёх ОС. Обе реализации возвращают «работает» одним
// и тем же способом, поэтому расхождение между ними молчит до живого прогона — как и в
// platform_env_test, тест объявлен вне ветки WIN32 и гоняется везде.
namespace {

int failures = 0;
void check(bool c, const char* what) {
    if (!c) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

struct WriteProbe {
    void* page;
};

// Запись в read-only маппинг. Вызывается ТОЛЬКО под guarded_call: смысл теста в том, что
// нарушение ловит ОС, а не соглашение, поэтому падение здесь — ожидаемый результат.
void poke(void* arg) {
    auto* probe = static_cast<WriteProbe*>(arg);
    static_cast<volatile uint32_t*>(probe->page)[0] = 0xDEADBEEFu;
}

} // namespace

int main() {
    // pid в имени — как у play_spawn_test: два параллельных прогона на одной машине иначе
    // отбирают сегмент друг у друга, и `second create ... refused` падает по чужой вине.
    const std::string name = "likenes_shm_" + std::to_string(platform::process_id());
    const size_t size = 4096;
    platform::SharedMemory::unlink(name); // хвост упавшего прогона (POSIX) — иначе O_EXCL валит

    // --- Создание + запись владельцем ---
    platform::SharedMemory owner;
    check(owner.open(name, size, /*create=*/true, /*writable=*/true), "owner creates writable segment");
    check(owner.valid() && owner.size() == size, "owner segment valid with requested size");
    auto* w = static_cast<uint32_t*>(owner.writable_data());
    check(w != nullptr, "writable mapping exposes writable_data()");
    if (w != nullptr)
        for (uint32_t i = 0; i < 16; ++i) w[i] = 0xA5A50000u + i;

    // --- O_EXCL-семантика: второе создание того же имени обязано провалиться ---
    platform::SharedMemory dup;
    check(!dup.open(name, size, /*create=*/true, /*writable=*/true),
          "second create with the same name is refused");

    // --- Присоединение по имени (путь дочернего процесса, решение 2) ---
    platform::SharedMemory reader;
    check(reader.open(name, size, /*create=*/false, /*writable=*/false),
          "reader attaches to existing segment by name");
    const auto* r = static_cast<const uint32_t*>(reader.data());
    bool same = (r != nullptr);
    for (uint32_t i = 0; same && i < 16; ++i)
        if (r[i] != 0xA5A50000u + i) same = false;
    check(same, "reader sees the bytes written by the owner");
    check(reader.writable_data() == nullptr, "read-only mapping refuses writable_data()");

    // --- Read-only обеспечен ОС, а не соглашением ---
    platform::install_crash_isolation();
    WriteProbe probe{const_cast<void*>(reader.data())};
    check(!platform::guarded_call(poke, &probe), "writing to the read-only mapping faults");
    // Прежнее содержимое цело: если бы запись прошла, зеркало инспектора могло бы менять sim.
    check(r != nullptr && r[0] == 0xA5A50000u, "read-only mapping unchanged after the faulting write");

    // --- Запрос размера больше фактического сегмента отвергается на всех трёх ОС ---
    // Без этой проверки POSIX замапил бы лишнее и упал SIGBUS'ом на первом обращении, а Windows
    // честно отказал бы: рассинхрон layout редактор↔игра вёл бы себя по-разному на разных ОС.
    // Превышение берётся с запасом в мегабайт, а не в пару страниц: обе ОС округляют фактический
    // размер сегмента вверх до страницы (на arm64-macOS она 16 КБ), и «size * 4» от четырёх
    // килобайт всё ещё умещалось бы внутри одной страницы — ассерт был бы вакуумным.
    platform::SharedMemory oversize;
    check(!oversize.open(name, size + (1u << 20), /*create=*/false, /*writable=*/false),
          "attaching with a size larger than the segment fails");

    // --- Несуществующее имя ---
    platform::SharedMemory missing;
    check(!missing.open("likenes_shm_absent", size, /*create=*/false, /*writable=*/false),
          "attaching to a missing segment fails");

    // --- Непортируемые имена отвергаются на обеих ОС одинаково ---
    platform::SharedMemory bad;
    check(!bad.open("", size, false, false), "empty name refused");
    check(!bad.open("/likenes_shm_test", size, false, false), "decorated name refused");
    check(!bad.open("dir\\name", size, false, false), "name with a separator refused");

    // --- Жизненный цикл: с последней ссылкой сегмент исчезает на обеих ОС ---
    reader.close();
    owner.close();
    platform::SharedMemory revived;
    check(!revived.open(name, size, /*create=*/false, /*writable=*/false),
          "segment is gone once owner and reader closed");

    const bool pass = (failures == 0);
    std::printf("platform-shmem: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
