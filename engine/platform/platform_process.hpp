#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Шов №2 платформы (спека #12, решение 2): запуск дочернего процесса. Реализации —
// platform_process_posix.cpp (fork/execvp) и platform_process_win32.cpp (CreateProcessW),
// выбор делает CMake, условной компиляции внутри нет.
//
// Потребители: пекарь (assetc дёргает пиннутые кодеки), build-loop и Play-spawn (#13).
namespace platform {

// Запускает argv[0] (поиск по PATH) с аргументами argv[1..], ждёт завершения.
// true — процесс завершился нормально с кодом возврата 0.
//
// stdout/stderr ребёнка уходят в /dev/null (NUL): инструменты пишут результат в файл, а их
// вывод только засоряет лог пекаря. Кому понадобится захват вывода — это отдельная функция,
// а не флаг: диагностики компилятора (#13) нужны построчно и с разбором.
bool run_tool(const std::vector<std::string>& argv);

// Дочерний процесс, которым управляют, а не просто ждут. Нужен гейтам живучести: убить пишущего
// на середине транзакции и посмотреть, что осталось на диске.
//
// В интерфейс сознательно не вынесен fork: на Windows его нет, и «убить копию себя» пришлось бы
// эмулировать. Поэтому ребёнок здесь ВСЕГДА отдельный процесс по argv — тест перезапускает
// собственный exe (platform::exe_path) со служебным флагом.
class Child {
public:
    Child() = default;
    ~Child();
    Child(Child&& o) noexcept;
    Child& operator=(Child&& o) noexcept;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;

    bool spawn(const std::vector<std::string>& argv);
    bool alive() const { return raw_ != 0; }

    // Убить немедленно и дождаться. true — процесс умер ИМЕННО от убийства: успей он выйти сам,
    // гейт «убили на середине записи» был бы вакуумным, и отличать эти два исхода обязан шов.
    bool kill_and_wait();

private:
    intptr_t raw_ = 0; // pid_t на POSIX, HANDLE на Windows
};

} // namespace platform
