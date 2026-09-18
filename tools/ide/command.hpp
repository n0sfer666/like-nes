#pragma once
#include "scene.hpp"
#include "serialize.hpp"
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// Command-bus (спека #7, гейт 2): все мутации сцены через bus → единый линейный undo-стек.
// Транзакция = группа команд (drag = 1 undo). Новая команда после undo обрубает redo-хвост.
namespace ide {

// `undo` возвращает исход, `redo` — нет, и разница не в симметрии: отменить шаг можно только из
// текста снимка, который мог быть написан другой сборкой, а повторить — тем же кодом, что уже
// отработал. То есть отказать способна ровно отмена (ревью аудита #21, A·2·8).
struct Command {
    std::function<void()> redo;
    std::function<bool()> undo;
};

class CommandBus {
public:
    explicit CommandBus(Scene& scene) : scene_(scene) {}
    CommandBus(const CommandBus&) = delete;
    CommandBus& operator=(const CommandBus&) = delete;

    void begin_group() {
        if (grouping_ && !group_buf_.cmds.empty()) done_.push_back(std::move(group_buf_));
        grouping_ = true;
        group_buf_ = Txn{};
    }
    void end_group() {
        grouping_ = false;
        if (!group_buf_.cmds.empty()) done_.push_back(std::move(group_buf_));
        group_buf_ = Txn{};
    }

    void execute(Command cmd) {
        cmd.redo();
        push(std::move(cmd));
    }

    flecs::entity create_entity(uint64_t guid) {
        if (scene_.exists(guid)) return scene_.get(guid);
        Command c;
        c.redo = [this, guid]() { scene_.create(guid); };
        c.undo = [this, guid]() { scene_.destroy(guid); return true; };
        execute(std::move(c));
        return scene_.get(guid);
    }

    void destroy_entity(uint64_t guid) {
        if (!scene_.exists(guid)) return;
        std::string snap = serialize_entity(scene_, guid);
        Command c;
        c.redo = [this, guid]() { scene_.destroy(guid); };
        // Исход отмены ВОЗВРАЩАЕТСЯ шине, а не выбрасывается. Прежде `restore_entity` звали ради
        // побочного действия: снимок, который не разобрался, оставлял сцену без сущности, а
        // история при этом шагала вперёд — то есть шина считала шаг отменённым, и redo предлагал
        // повторить то, чего не было (ревью аудита #21, A·2·8).
        c.undo = [this, guid, snap]() {
            std::string why;
            if (restore_entity(scene_, guid, snap, &why)) return true;
            // Причина отказа иначе не доходит НИКУДА: у `Command::undo` канала для текста нет, а
            // исход шина отдаёт булевым. Владелец при этом видит Undo, который просто не сработал,
            // и ни слова о том, почему — самая дорогая форма отказа из возможных (ревью, A·2·8).
            std::fprintf(stderr, "undo refused: %s\n", why.c_str());
            return false;
        };
        execute(std::move(c));
    }

    template <typename T>
    void set_component(uint64_t guid, const T& value) {
        if (!scene_.exists(guid)) return;
        const T* old = scene_.get(guid).try_get<T>();
        bool had = old != nullptr;
        T oldval = had ? *old : T{};
        T newval = value;
        Command c;
        c.redo = [this, guid, newval]() { scene_.get(guid).set<T>(newval); };
        c.undo = [this, guid, had, oldval]() {
            flecs::entity e = scene_.get(guid);
            if (had) e.set<T>(oldval);
            else e.remove<T>();
            return true;
        };
        execute(std::move(c));
    }

    bool can_undo() const { return !done_.empty(); }
    bool can_redo() const { return !undone_.empty(); }
    size_t undo_depth() const { return done_.size(); }
    size_t redo_depth() const { return undone_.size(); }

    // `false` = отмена НЕ состоялась, и шаг остался ЦЕЛЫМ: транзакция не уезжает в `undone_` (redo
    // по наполовину отменённому шагу повторил бы действие поверх состояния, которого не было) и
    // возвращается в `done_`, а те команды, что успели отмениться, накатываются назад через
    // `redo()`. Прежде отказ терял транзакцию вовсе: группа оставалась разобранной наполовину, и
    // повторить отмену было уже нечем — `can_undo()` про неё не знал (решение владельца, A·2·8).
    //
    // `[[nodiscard]]` потому, что выброшенный исход — это ровно тот дефект, ради которого возврат
    // и заводился: история, шагнувшая вперёд по несостоявшейся отмене.
    [[nodiscard]] bool undo() {
        if (done_.empty()) return false;
        Txn t = std::move(done_.back());
        done_.pop_back();
        // `n` — сколько команд отменилось ДО отказа; инкремент в шаге цикла, поэтому выход по
        // отказу оставляет в нём число успешных, а не номер сорвавшейся.
        size_t n = 0;
        for (auto it = t.cmds.rbegin(); it != t.cmds.rend(); ++it, ++n) {
            if (it->undo()) continue;
            // Отменяли с конца — значит успели последние `n`; накатываем их обратно с начала
            // хвоста, в том же порядке, в каком они выполнялись изначально.
            for (size_t i = 0; i < n; ++i) t.cmds[t.cmds.size() - n + i].redo();
            done_.push_back(std::move(t));
            return false;
        }
        undone_.push_back(std::move(t));
        return true;
    }

    void redo() {
        if (undone_.empty()) return;
        Txn t = std::move(undone_.back());
        undone_.pop_back();
        for (auto& c : t.cmds) c.redo();
        done_.push_back(std::move(t));
    }

private:
    struct Txn { std::vector<Command> cmds; };

    void push(Command cmd) {
        undone_.clear();
        if (grouping_) {
            group_buf_.cmds.push_back(std::move(cmd));
        } else {
            Txn t;
            t.cmds.push_back(std::move(cmd));
            done_.push_back(std::move(t));
        }
    }

    Scene& scene_;
    std::vector<Txn> done_;
    std::vector<Txn> undone_;
    bool grouping_ = false;
    Txn group_buf_;
};

} // namespace ide
