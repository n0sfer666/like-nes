#pragma once
#include <string>
#include <vector>

// Шов №8 платформы (спека #13, решение 4): наблюдение за каталогом. Реализации —
// platform_watch_linux.cpp (inotify), platform_watch_macos.cpp (FSEvents),
// platform_watch_win32.cpp (ReadDirectoryChangesW), выбор делает CMake.
//
// Форма API — опрос, а не колбэк: build-loop дёргает watcher из своего цикла и обязан знать,
// КОГДА он смотрит на изменения. Колбэк с потока ОС (у FSEvents и RDCW он свой) тащил бы
// синхронизацию в каждого потребителя и размывал момент пересборки.
//
// Пути в changed — абсолютные (каталог наблюдения + имя), UTF-8. Одно изменение способно
// приехать несколькими событиями (запись → rename → chmod у типового «сохранить» редактора),
// поэтому пути внутри одного poll дедуплицируются: пересобирать один файл трижды незачем.
namespace platform {

// Какой бэкенд отвечает НА САМОМ ДЕЛЕ. Нативный может не подняться на ровном месте — сетевая ФС,
// shared folder виртуалки, исчерпанный лимит inotify-вотчей, — и тогда шов молча уходит в
// поллинг. Молча для кода, но не для глаз: гейт 3 спеки прогоняется в том числе на shared-folder
// VM, и «какой бэкенд там был» — часть результата, а не догадка.
enum class WatchBackend {
    Native,
    Poll,
};

class Watcher {
public:
    Watcher() = default;
    ~Watcher();
    Watcher(const Watcher&) = delete;
    Watcher& operator=(const Watcher&) = delete;
    Watcher(Watcher&& o) noexcept { steal(o); }
    Watcher& operator=(Watcher&& o) noexcept {
        if (this != &o) {
            close();
            steal(o);
        }
        return *this;
    }

    // Начать наблюдение. Вызывается один раз на объект: второй вызов — ошибка, а не добавление
    // второго каталога (нативные бэкенды по-разному считают вотчи, и «добавить» на Windows это
    // отдельный хендл с отдельным ожиданием).
    //
    // recursive=true покрывает и подкаталоги, СОЗДАННЫЕ после старта: inotify этого сам не умеет,
    // и без дорегистрации новый каталог исходников оказался бы немым.
    //
    // false — каталога нет либо объект уже наблюдает; error() говорит что именно. Отказ ИМЕННО
    // нативного бэкенда сюда не попадает: он переводит объект в поллинг и возвращает true.
    bool watch_dir(const std::string& dir, bool recursive);

    // Ждёт до timeout_ms и отдаёт изменившиеся пути. 0 — опрос без ожидания. Пустой changed при
    // true — «за окно ничего не произошло», а не ошибка: цикл редактора тикает и без правок.
    // false — наблюдение сломалось (дескриптор умер); changed при этом пуст.
    // changed очищается в начале, чтобы прошлое окно не выглядело текущим.
    bool poll(std::vector<std::string>& changed, int timeout_ms);

    void close();

    // Непрозрачные состояния бэкендов. Публичны только именем: определения живут в
    // соответствующих .cpp, и собрать или разыменовать их снаружи нечем. Приватными им быть
    // нельзя — на них ссылаются свободные функции detail:: и колбэк ОС.
    struct Native;
    struct Polling;

    bool valid() const { return native_ != nullptr || poll_ != nullptr; }
    WatchBackend backend() const { return backend_; }
    const std::string& error() const { return error_; }

private:
    void steal(Watcher& o) noexcept {
        native_ = o.native_;
        poll_ = o.poll_;
        backend_ = o.backend_;
        error_ = std::move(o.error_);
        o.native_ = nullptr;
        o.poll_ = nullptr;
    }

    Native* native_ = nullptr;
    Polling* poll_ = nullptr;
    WatchBackend backend_ = WatchBackend::Poll;
    std::string error_;
};

namespace detail {

// Фолбэк-поллинг: общий на три ОС, потому что собран из platform_fs (list_dir + file_stamp) и
// системных вызовов не знает вовсе. Владеет им Watcher, здесь — только фабрика и шаги, чтобы
// платформенные TU не повторяли обход дерева трижды.
Watcher::Polling* poll_open(const std::string& dir, bool recursive);
void poll_close(Watcher::Polling* p);
bool poll_step(Watcher::Polling* p, std::vector<std::string>& changed, int timeout_ms);

// Ручка «принудительно поллинг»: LIKE_NES_WATCH=poll. Нужна гейту 3 — на shared-folder VM
// нативный бэкенд может подняться и при этом терять события, а такой прогон нужно уметь
// повторить намеренно, а не ловить удачу.
bool poll_forced();

} // namespace detail

} // namespace platform
