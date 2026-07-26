#pragma once
#include <string>
#include <utility>

// Шов №3 платформы (спека #12, решение 2): загрузка нативного модуля. Реализации —
// platform_module_posix.cpp (dlopen) и platform_module_win32.cpp (LoadLibraryW + копия),
// выбор делает CMake, условной компиляции по ОС внутри нет.
//
// Копия перед загрузкой (решение 4 владельца) — не деталь реализации, а условие горячей
// перезагрузки: Windows держит загруженный образ файлом-секцией, и пересборка плагина поверх
// живого .dll падает с отказом доступа. Грузим копию — оригинал остаётся перезаписываемым.
// На POSIX копия не нужна и не делается: dlopen не мешает перезаписать файл.
//
// Следствие, на которое НЕЛЬЗЯ полагаться в обратную сторону: одно открытие = один экземпляр.
// Два open() одного пути дают на Windows две независимые копии со СВОЕЙ статикой, а на POSIX —
// один образ с общим refcount'ом. Переносимого «открой ещё раз и увидишь то же состояние» нет;
// кому нужен уже загруженный модуль, тот обязан спросить у владельца хендла (PluginHost::symbol),
// а не открывать путь повторно. Уравнивать поведение кэшем path→handle сознательно не стали:
// он вернул бы старый образ после пересборки и убил бы hot-reload ради семантики, нужной тестам.
namespace platform {

class Module {
public:
    Module() = default;
    ~Module() { close(); }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    Module(Module&& o) noexcept : handle_(o.handle_), temp_(std::move(o.temp_)) {
        o.handle_ = nullptr;
        o.temp_.clear();
    }
    Module& operator=(Module&& o) noexcept {
        if (this != &o) {
            close();
            handle_ = o.handle_;
            temp_ = std::move(o.temp_);
            o.handle_ = nullptr;
            o.temp_.clear();
        }
        return *this;
    }

    bool open(const std::string& utf8_path);
    void close();

    // Забыть модуль, НЕ выгружая: под ASan выгрузка стирает символы, и стек падения в уже
    // выгруженном плагине становится нечитаемым (крэш-изоляция #6). Цена — временная копия
    // на Windows остаётся лежать; это путь только для санитайзерных сборок.
    void detach() {
        handle_ = nullptr;
        temp_.clear();
    }

    void* symbol(const char* name) const;
    bool valid() const { return handle_ != nullptr; }

    // Причина последней неудачи open() ИЛИ symbol() в этом потоке: оба шва обязаны её заполнять,
    // иначе диагностика hot-reload врёт текстом от прошлой ошибки. Валидна до следующей неудачи.
    static const char* last_error();

private:
    void* handle_ = nullptr;
    std::string temp_; // путь копии, которую надо убрать при close() (пусто на POSIX)
};

} // namespace platform
