#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Шов №7 платформы (спека #13, решение 1): именованная разделяемая память. Реализации —
// platform_shmem_posix.cpp (shm_open + mmap) и platform_shmem_win32.cpp (CreateFileMappingW +
// MapViewOfFile), выбор делает CMake, условной компиляции внутри нет.
//
// Имя на входе — ПОРТИРУЕМЫЙ токен без разделителей (`likenes_ide_1234`), а не готовый путь:
// POSIX требует ведущий `/`, Windows — префикс пространства имён `Local\`, и декорирование живёт
// в реализации. Каллер, передающий украшенное имя, работал бы ровно на одной ОС.
//
// Наследование дескриптора в дочерний процесс не используется (решение 2 спеки): ребёнок
// открывает сегмент ПО ИМЕНИ из своей командной строки.
//
// Отсюда единственная асимметрия полей: POSIX закрывает дескриптор сразу после mmap (имя живёт
// в /dev/shm до shm_unlink независимо от него — приём MappedFile), а Windows ОБЯЗАН держать
// хендл секции открытым всё время жизни. View там продлевает жизнь памяти, но не ИМЕНИ: с
// последним хендлом объект уходит из каталога имён, и OpenFileMappingW у ребёнка вернёт NULL,
// хотя страницы ещё живы. Симметрия полей здесь стоила бы работоспособности на одной из ОС.
namespace platform {
namespace detail {

// Общая валидация имени: правило одно на обе ОС, декорирование — разное. Потолок в 30 символов
// диктует macOS (PSHMNAMLEN = 31 вместе с ведущим `/`) — самый узкий из трёх; принимать более
// длинное имя значило бы ловить отказ только на одной ОС из трёх.
inline bool shmem_name_ok(const std::string& n) {
    if (n.empty() || n.size() > 30) return false;
    for (char c : n)
        if (c == '/' || c == '\\' || c == '\0') return false;
    return true;
}

} // namespace detail

class SharedMemory {
public:
    SharedMemory() = default;
    ~SharedMemory() { close(); }
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&& o) noexcept { steal(o); }
    SharedMemory& operator=(SharedMemory&& o) noexcept {
        if (this != &o) {
            close();
            steal(o);
        }
        return *this;
    }

    // create=true — сегмент обязан не существовать (O_EXCL-семантика на обеих ОС): молчаливое
    // присоединение к чужому сегменту с тем же именем дало бы гейту 1 чужие данные вместо своих.
    // writable=false — маппинг read-only СРЕДСТВАМИ ОС: запись инспектора в sim физически
    // невозможна, а не запрещена соглашением (инвариант детерминизма #7).
    bool open(const std::string& name, size_t size, bool create, bool writable);
    void close();

    // Два аксессора вместо одного не для удобства: writable_data() возвращает nullptr на
    // read-only маппинге, поэтому «писать в зеркало» не компилируется там, где не разрешено.
    // Он же неконстантный — `const SharedMemory&` не должен быть мандатом на запись в сегмент.
    const void* data() const { return addr_; }
    void* writable_data() { return writable_ ? addr_ : nullptr; }
    size_t size() const { return size_; }
    bool valid() const { return addr_ != nullptr; }

    // Форс-очистка осиротевшего сегмента перед create. На Windows — no-op: секция там живёт по
    // счётчику ссылок и исчезает с последним хендлом, осиротеть ей нечем.
    static void unlink(const std::string& name);

private:
    void steal(SharedMemory& o) noexcept {
        addr_ = o.addr_;
        size_ = o.size_;
        name_ = std::move(o.name_);
        native_ = o.native_;
        owner_ = o.owner_;
        writable_ = o.writable_;
        o.addr_ = nullptr;
        o.size_ = 0;
        o.native_ = 0;
        o.owner_ = false;
        o.writable_ = false;
    }

    void* addr_ = nullptr;
    size_t size_ = 0;
    std::string name_;
    // HANDLE секции на Windows (см. преамбулу); POSIX-реализация поле не трогает и держит 0.
    // Хранится как intptr_t, чтобы <windows.h> не протёк в заголовок, который включают все.
    intptr_t native_ = 0;
    bool owner_ = false;
    bool writable_ = false;
};

} // namespace platform
