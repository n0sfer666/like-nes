#pragma once
#include <cstdint>

namespace framework::input {

inline constexpr uint16_t NO_ACTION = 0xffffu;

struct ActionBuffer {
    uint16_t action = NO_ACTION;
    uint16_t ticks = 0;
};

void clear_action(ActionBuffer& buf);
// Нулевой срок кладёт пустой буфер — по той же причине, по которой истёкший обязан совпасть с пустым.
void buffer_action(ActionBuffer& buf, uint16_t action, uint16_t ticks);

// Срок тратится только на тиках, где действие начать не удалось. Истёкший буфер обязан совпасть с
// пустым: буфер входит в хеш состояния игры, и висящее при нулевом сроке действие развело бы равные миры.
// `use` не кладёт действие в тот же буфер: после успешного `use` буфер сбрасывается, и положенное пропадёт.
template <typename Use>
bool take_action(ActionBuffer& buf, Use use) {
    if (buf.ticks == 0) return false;
    if (use(buf.action)) {
        clear_action(buf);
        return true;
    }
    if (--buf.ticks == 0) clear_action(buf);
    return false;
}

} // namespace framework::input
