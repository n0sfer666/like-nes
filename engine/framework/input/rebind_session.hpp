#pragma once
#include <cstdint>

#include "action_map.hpp"
#include "presets.hpp"
#include "rebind_store.hpp"

// Режим «нажми клавишу»: сессия ловит ОДНО сырое button-down событие и превращает его в источник.
// Слушается сырой поток, а не разрешённые действия, — иначе назначить кнопку, уже занятую другим
// действием, было бы нельзя (её съел бы владелец), а это ровно тот случай, ради которого перебинд
// и открывают.
namespace framework::input {

// Кто уже занимает источник. action < 0 — свободен.
struct RebindConflict {
    int action = -1;
    uint32_t which = 0;
};

// Источник действия С УЧЁТОМ накладки: пресет — база, RebindStore — верхний слой.
bool effective_source(const PresetTable& table, uint32_t preset, const RebindStore& store,
                      uint32_t action, uint32_t which, ::input::Source& out);

// Первое действие пресета, чей действующий источник совпадает с src. except_action исключается:
// переназначить кнопку на неё же — не конфликт.
RebindConflict find_conflict(const PresetTable& table, uint32_t preset, const RebindStore& store,
                             const ::input::Source& src, int except_action);

class RebindSession {
public:
    void begin(int action, uint32_t which);
    void cancel();
    bool active() const { return active_; }
    bool captured() const { return captured_; }
    int action() const { return action_; }
    uint32_t which() const { return which_; }
    ::input::Source candidate() const { return candidate_; }

    // true — событие захвачено и сессия ждёт подтверждения. Оси и key-up игнорируются: отпускание
    // клавиши, которой открыли меню, иначе назначилось бы само.
    bool feed(const ::input::RawEvent& e);

    // Записать захваченный источник в накладку. Занятый источник отдаётся конфликтом и НЕ
    // применяется; force снимает его с прежнего владельца (у того остаётся действие без этого
    // биндинга — молча оставить одну кнопку на два действия хуже).
    bool commit(const PresetTable& table, uint32_t preset, RebindStore& store, bool force,
                RebindConflict* conflict);

private:
    bool active_ = false;
    bool captured_ = false;
    int action_ = -1;
    uint32_t which_ = 0;
    ::input::Source candidate_;
};

} // namespace framework::input
