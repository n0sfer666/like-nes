#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "platformer_net_hooks_witness.hpp"
#include "platformer_peer.hpp"
#include "platformer_peer_argv.hpp"
#include "platformer_peer_hooks.hpp"
#include "platformer_sim.hpp"

// Заглушка живой половины гейта 9 (спека #22): ЧЕМ двигают пира, а не ЧТО о нём утверждают.
// Отдельно от самих утверждений по той же причине, что и оснастка в `platformer_net_runs.hpp`:
// в общем файле каждая правка режима читалась бы как правка гейта.
//
// Хуки здесь заглушки нарочно: ввод берётся из того же скрипта, показ ничего не рисует. Предмет
// гейта — проводка шва, а не GLFW. Режимы — не игра, а СЛОМАННЫЕ и УКОРОЧЕННЫЕ реализации под
// контроли: без них «через шов то же самое» верно и для `run_peer`, который шов не спрашивает
// вовсе, а числа, названные швом, гейт принимал бы не глядя, пока они равны headless-умолчаниям.
namespace platformer::fake {

// Тик, на котором контроль кэша подставляет отказ отправки. Внутри первой полосы скрипта («бежит
// вправо»), а не где попало: на тике с ПУСТЫМ вводом потерянный сэмпл равен взятому, и сломанная
// реализация сошлась бы с эталоном, ничего не сказав про кэш.
constexpr int64_t STALL_AT = 7;

struct Plan {
    bool idle = false;    // отправитель стоит на месте
    bool stop = false;    // показ отказывает сразу
    bool slow = false;    // ввод отдаётся через раз
    bool fickle = false;  // повторный вопрос о том же тике возвращает пустоту
    bool forget = false;  // взятый сэмпл выбрасывается вместе с отказом отправки
    int64_t stall = -1;   // тик, на котором отправка отказывает один раз
    uint32_t horizon = 0; // 0 — скриптовая длина
    int64_t deadline = 0; // 0 — headless-умолчание
};

// Незнакомый режим есть ОТКАЗ, а не «играем как обычно»: опечатка в нём тихо давала бы прогон БЕЗ
// хуков, то есть зелёный гейт, не проверивший шов ни разу.
inline bool plan_of(const char* mode, Plan& out) {
    if (std::strcmp(mode, "same") == 0) return true;
    if (std::strcmp(mode, "idle") == 0) { out.idle = true; return true; }
    if (std::strcmp(mode, "stop") == 0) { out.stop = true; return true; }
    if (std::strcmp(mode, "slow") == 0) { out.slow = true; return true; }
    // Пара под кэш взятого сэмпла: обеим отправка отказывает на `STALL_AT`, и обе на повторный
    // вопрос о том же тике отдают пустоту — как человек, чьи фронты клавиш кадра уже съедены.
    // Расходятся они одним: `dropped` кэш не держит, и прогон обязан уехать от эталона.
    if (std::strcmp(mode, "held") == 0) {
        out.stall = STALL_AT;
        out.fickle = true;
        return true;
    }
    if (std::strcmp(mode, "dropped") == 0) {
        out.stall = STALL_AT;
        out.fickle = true;
        out.forget = true;
        return true;
    }
    // Половина скрипта и одна миллисекунда — числа, которых у headless-пира нет: горизонт он берёт
    // у `script_ticks()`, дедлайн — у `PEER_DEADLINE_MS`, и оба тернарника в `run_peer` остались бы
    // непроверенными, отвечай шов теми же величинами.
    if (std::strcmp(mode, "short") == 0) { out.horizon = script_ticks() / 2; return true; }
    if (std::strcmp(mode, "impatient") == 0) { out.deadline = 1; return true; }
    std::printf("peer: this --hooks mode was not understood: %s\n", mode);
    return false;
}

class ScriptHooks final : public PeerHooks {
public:
    explicit ScriptHooks(const Plan& plan) : plan_(plan) {}

    uint32_t horizon() const override {
        return plan_.horizon != 0 ? plan_.horizon : script_ticks();
    }
    int64_t deadline_ms() const override {
        return plan_.deadline != 0 ? plan_.deadline : PEER_DEADLINE_MS;
    }
    bool input(uint32_t tick, ch::MoveInput& out) override {
        // «Ещё не готов» — законный ответ живой половины, и отвечает она им чаще, чем отдаёт ввод:
        // кадр экрана длиннее прохода цикла. Через раз, а не всегда: пир, которому не отдали ввод
        // ни разу, уходит по дедлайну, и ветка «взял и уехал» осталась бы непроверенной.
        const uint32_t nth = seen_.asked++;
        if (plan_.slow && (nth % 2) == 0) {
            ++seen_.refused;
            return false;
        }
        // Второй вопрос о ТОМ ЖЕ тике возвращает пустоту: у человека фронты клавиш этого кадра
        // съедены первым чтением, и пир, не сохранивший взятый сэмпл, сыграет не то, что игралось.
        const bool again = plan_.fickle && nth != 0 && tick == last_;
        last_ = tick;
        out = plan_.idle || again ? ch::MoveInput{} : script_input(tick);
        return true;
    }
    bool show(const Stage&) override {
        ++seen_.shown;
        return !plan_.stop;
    }

    const Witness& seen() const { return seen_; }

private:
    Plan plan_;
    Witness seen_;
    uint32_t last_ = 0;
};

// Вход в роль пира под режимом гейта. Разбор `--peer` тот же строгий, что у живой цели: знать ему
// про тестовую заглушку нечего, поэтому флаг режима вырезается из argv ДО него.
inline int run_fake_peer(int argc, char** argv, const char* mode) {
    PeerConfig cfg;
    if (!parse_peer(argc, argv, cfg)) return 3;
    Plan plan;
    // Свой код, а не 3: кодом 3 отвечает и рандеву, не дождавшееся соседа, и утверждение об отказе
    // режима сходилось бы на заглушке, ТИХО принявшей незнакомую строку, — по чужой причине и
    // восемью секундами позже. Кода 11 `run_peer` не возвращает никогда (`platformer_peer.hpp`).
    if (!plan_of(mode, plan)) return 11;
    ScriptHooks hooks(plan);
    cfg.hooks = &hooks;
    cfg.stall_send_tick = plan.stall;
    cfg.forget_stalled = plan.forget;
    const int code = run_peer(cfg);
    // Улика пишется и после ОТКАЗА: ранний выход — это как раз ненулевой код, и утверждение о нём
    // судит по числу показанных кадров.
    if (!write_witness(cfg.prefix, cfg.sender, hooks.seen())) {
        std::printf("peer %s: the hooks witness was not written\n", cfg.sender ? "send" : "recv");
    }
    return code;
}

} // namespace platformer::fake
