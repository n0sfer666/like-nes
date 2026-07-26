#pragma once
#include "def.hpp"
#include "manifest.hpp"
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

namespace ach {

class Registry {
public:
    DefineResult define(const DefSpec& spec);
    DefineResult define_stat(const char* key);

    const Entry* find(Id id) const;
    const Stat* find_stat(Id id) const;
    std::size_t stat_index(Id id) const;

    const std::vector<Entry>& entries() const { return entries_; }
    const std::vector<Stat>& stats() const { return stats_; }

    // Загрузка манифеста атомарна: строки adopted-записей указывают внутрь mmap-региона, и при
    // отказе на N-й записи вызывающий размапит его — реестр обязан откатиться к состоянию до старта.
    class Transaction {
    public:
        explicit Transaction(Registry& reg);
        ~Transaction();
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        void commit() { done_ = true; }

    private:
        Registry& reg_;
        std::vector<Entry> entries_;
        std::vector<Stat> stats_;
        bool done_ = false;
    };

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

private:
    // adopt* сохраняют указатели вызывающего ДОСЛОВНО (строки живут в чужой памяти — mmap-регионе
    // бандла), поэтому единственный легальный вызывающий — load_manifest под Transaction.
    DefineResult adopt(const Def& def, const char* key, const char* name, const char* desc);
    DefineResult adopt_stat(Id id, const char* key);
    friend LoadResult load_manifest(Registry& reg, const void* base, std::size_t size);

    const char* intern(const char* s);

    std::deque<std::string> arena_;
    std::vector<Entry> entries_;
    std::vector<Stat> stats_;
};

} // namespace ach
