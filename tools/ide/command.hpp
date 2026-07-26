#pragma once
#include "scene.hpp"
#include "serialize.hpp"
#include <cstdint>
#include <functional>
#include <vector>

// Command-bus (спека #7, гейт 2): все мутации сцены через bus → единый линейный undo-стек.
// Транзакция = группа команд (drag = 1 undo). Новая команда после undo обрубает redo-хвост.
namespace ide {

struct Command {
    std::function<void()> redo;
    std::function<void()> undo;
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
        c.undo = [this, guid]() { scene_.destroy(guid); };
        execute(std::move(c));
        return scene_.get(guid);
    }

    void destroy_entity(uint64_t guid) {
        if (!scene_.exists(guid)) return;
        std::string snap = serialize_entity(scene_, guid);
        Command c;
        c.redo = [this, guid]() { scene_.destroy(guid); };
        c.undo = [this, guid, snap]() { restore_entity(scene_, guid, snap); };
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
        };
        execute(std::move(c));
    }

    bool can_undo() const { return !done_.empty(); }
    bool can_redo() const { return !undone_.empty(); }
    size_t undo_depth() const { return done_.size(); }
    size_t redo_depth() const { return undone_.size(); }

    void undo() {
        if (done_.empty()) return;
        Txn t = std::move(done_.back());
        done_.pop_back();
        for (auto it = t.cmds.rbegin(); it != t.cmds.rend(); ++it) it->undo();
        undone_.push_back(std::move(t));
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
