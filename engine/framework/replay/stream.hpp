#pragma once
#include <cstdint>
#include <vector>

// Поток реплея: ввод всех игроков на тик и ЗАЯВЛЕННЫЙ хеш состояния после этого тика (спека #22,
// вертикаль 2, гейт 6).
//
// Хеш на КАЖДЫЙ тик, а не один на прогон, и это решение, а не запас. Гейт требует назвать номер
// ПЕРВОГО разошедшегося тика, а из одного финального хеша номер не выводится ничем: он говорит
// «где-то на четырёхстах тиках», то есть ровно то, что и так известно по факту отказа. Цена
// решения — восемь байт на тик; цена отказа от него — верификатор, который ловит подделку и не
// умеет сказать, что именно подделано.
//
// Сети здесь нет ни строкой, как и в `rollback`: поток есть запись прогона, а приехал он по сети,
// из файла или из соседнего процесса — верификации не касается.
namespace framework::replay {

using Tick = uint32_t;

template <class Input>
class Stream {
public:
    // Игроков ноль — это не пустой поток, а поток, у которого нет строки: `record` на нём обязан
    // отказать, а не молча писать нулевую ширину.
    void reset(uint32_t players) {
        players_ = players;
        rows_.clear();
        claims_.clear();
    }

    bool record(const Input* row, uint64_t claim) {
        if (players_ == 0 || row == nullptr) return false;
        rows_.insert(rows_.end(), row, row + players_);
        claims_.push_back(claim);
        return true;
    }

    uint32_t players() const { return players_; }
    Tick ticks() const { return static_cast<Tick>(claims_.size()); }

    // Строка тика — подряд по игрокам, ровно `players()` штук: тот же контракт, что у `Sim::step`,
    // потому что она в него и уходит.
    const Input* row(Tick t) const { return rows_.data() + static_cast<size_t>(t) * players_; }
    uint64_t claim(Tick t) const { return claims_[t]; }

    // Ручки подделки — для гейта, и живут они ЗДЕСЬ, а не в нём: подделка обязана менять ровно
    // одно поле ровно одного тика, а гейт, лезущий в приватные вектора, проверял бы заодно и свою
    // арифметику индексов.
    bool forge_input(Tick t, uint32_t player, const Input& in) {
        if (t >= ticks() || player >= players_) return false;
        rows_[static_cast<size_t>(t) * players_ + player] = in;
        return true;
    }

    bool forge_claim(Tick t, uint64_t claim) {
        if (t >= ticks()) return false;
        claims_[t] = claim;
        return true;
    }

private:
    uint32_t players_ = 0;
    std::vector<Input> rows_;
    std::vector<uint64_t> claims_;
};

} // namespace framework::replay
