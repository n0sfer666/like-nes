#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "stage.hpp"

// Расписание систем фреймворка: стадия + явные зависимости → детерминированный порядок.
//
// Почему не «в порядке регистрации»: модули слоя регистрируются из разных TU, а порядок
// статической инициализации между TU стандарт не определяет — он зависит от порядка объектов на
// линкер-строке. Расписание, читающее порядок регистрации, давало бы разный sim-хеш от
// перестановки библиотек в CMake, и найти это можно было бы только сравнением двух сборок.
// Поэтому порядок восстанавливается из данных: топосортировка внутри стадии с разрывом ничьих
// по имени. Одно и то же множество систем даёт одну и ту же последовательность всегда.
namespace framework {

struct SystemDesc {
    const char* name = nullptr;             // уникальный ключ; по нему же ссылаются зависимости
    Stage stage = Stage::Sim;
    SystemFn fn = nullptr;
    void* user = nullptr;
    const char* const* after = nullptr;     // имена систем, после которых обязана идти эта
    std::size_t after_count = 0;
};

enum class BuildResult : uint32_t {
    Ok = 0,
    BadSystem = 1,       // нет имени или функции
    DuplicateName = 2,
    UnknownDependency = 3,
    LaterStageDependency = 4, // зависимость из более поздней стадии — неудовлетворима по построению
    Cycle = 5,
};

const char* build_reason(BuildResult r);

class Schedule {
public:
    // Регистрация. Дубликаты и битые описания отвергаются здесь же, чтобы ошибка называла систему,
    // а не всплывала обезличенной на build().
    bool add(const SystemDesc& d);

    // Разложить в порядок исполнения. Зовётся один раз при старте: аллокации живут здесь, run()
    // их уже не делает.
    BuildResult build();

    void run(const Tick& t) const;
    void run_stage(Stage s, const Tick& t) const;

    // Имя системы, на которой разбор остановился, — иначе «цикл» не подсказывает, где искать.
    const std::string& error_system() const { return error_system_; }

    // Порядок исполнения наружу: гейт сравнивает последовательность имён, а не наблюдаемый эффект.
    std::size_t size() const { return order_.size(); }
    const char* name_at(std::size_t i) const;
    bool built() const { return built_; }

private:
    struct Entry {
        SystemDesc desc;
        std::string name;
        std::vector<std::size_t> deps;  // индексы в systems_ внутри той же стадии
    };

    std::vector<Entry> systems_;
    std::vector<std::size_t> order_;    // индексы systems_ в порядке исполнения
    std::size_t stage_end_[STAGE_COUNT] = {};  // границы стадий в order_ (для run_stage)
    std::string error_system_;
    bool built_ = false;
};

} // namespace framework
