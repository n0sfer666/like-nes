#include "wasm_host.hpp"
#include "wasm_escape.hpp"
// Константы бака спрашиваются здесь напрямую (`WASM_FUEL_PER_CALL`, `WasmFuel`), и приезжать они
// обязаны своим включением: транзитивно через зонд побега они уехали бы вместе с ним, а этот гейт
// живёт отдельной целью ровно затем, чтобы от зонда не зависеть (ревью A·2).
#include "wasm_sandbox.hpp"
#include "sim.hpp"
#include <cstdio>
#include <string>

// Контракт ОБЪЕКТА WasmGravity, а не гейт побега: тот живёт в `plugin_wasm_test.cpp` и спрашивает
// у чужого модуля, что ему разрешили. Здесь спрашивают у хозяина, что он говорит О СЕБЕ — какую
// причину отказа он сохраняет, что отвечает на повторную попытку и какую цифру расхода называет
// после неудачного вызова. Все ответы до аудита #21 были неверны (ревью A·2).

// Расход ПОСЛЕДНЕГО вызова, а не последнего удачного: гость считает свои вызовы и трапает на
// втором, накрутив перед трапом тысячу витков. Тысяча — чтобы расход второго вызова был ЗАМЕТНО
// больше расхода первого: сравняйся они, ассерт не отличил бы «свой расход» от застрявшей цифры
// первого вызова, а трап без витков стоит ровно столько же, сколько честный проход (проба v26).
static const char* TWICE_WAT =
    "(module (memory (export \"mem\") 1) (global $c (mut i32) (i32.const 0))"
    " (func (export \"gravity\") (param i32 i32 i32 i32) (local $i i32)"
    " (global.set $c (i32.add (global.get $c) (i32.const 1)))"
    " (if (i32.gt_u (global.get $c) (i32.const 1)) (then"
    " (local.set $i (i32.const 1000))"
    " (loop $l (local.set $i (i32.sub (local.get $i) (i32.const 1))) (br_if $l (local.get $i)))"
    " (unreachable)))))";

// Гость в бесконечном цикле: единственный случай, где расход РАВЕН баку, и ровно ради него счётчик
// заводился. Ноль на этом месте не отличим от «вызова не было» (аудит #21, ревью A·2).
static const char* SPIN_WAT =
    "(module (memory (export \"mem\") 1)"
    " (func (export \"gravity\") (param i32 i32 i32 i32) (loop $l (br $l))))";

// Модуль без экспортов движка: `init_wat` обязан отказать, не оставив объект мёртвым навсегда.
static const char* BARE_WAT = "(module)";

// Память меньше мира: ЕДИНСТВЕННЫЙ отказ `apply()`, после которого хозяин остаётся живым — до
// вызова гостя дело не доходит, трапа нет, выключать некого. Ровно на нём проверяется, что
// причина отказа называет себя и что расход не выдумывается там, где вызова не было (ревью A·2).
static const char* SMALL_WAT =
    "(module (memory (export \"mem\") 0)"
    " (func (export \"gravity\") (param i32 i32 i32 i32)))";

int main() {
    SimWorld w;
    sim_init(w);
    const int32_t g_raw = fix32::from_float(9.8).raw;
    const int32_t dt_raw = fix32::from_float(1.0 / 60.0).raw;

    // Отказ чтения файла — СЕДЬМАЯ ветка отказа `init()`, и причина обязана дожить до `error()`.
    // Пока Impl создавался после чтения, объект оставался с `p_ == nullptr`, и первый же `apply()`
    // затирал «cannot read <путь>» словами «not initialized» ровно там, где причину читают.
    WasmGravity g;
    const bool no_file = !g.init("engine/plugin/no-such-plugin.wat");
    const bool silent = !g.apply(w, g_raw, dt_raw);
    const bool kept = g.error().rfind("cannot read", 0) == 0;

    // Повторная попытка после ПРОВАЛИВШЕЙСЯ инициализации: сторож по `p_` отвечал бы «already
    // initialized» объекту, который ни разу не собрался, и причина провала называлась бы чужой.
    const bool bad_wat = !g.init_wat(BARE_WAT);
    const bool retried = g.init_wat(TWICE_WAT);
    // Удачный заход обязан забрать слово прошлого отказа: иначе живой объект отвечает «cannot
    // read <путь>» про плагин, который прекрасно встал.
    const bool err_cleared = retried && g.error().empty();

    const bool first_ok = retried && g.apply(w, g_raw, dt_raw);
    const uint64_t first_fuel = g.fuel_used();
    // Удачный вызов обязан забрать слово прошлого отказа, трапнувший — назвать СЕБЯ: под общим
    // молчанием оба конца контракта `error()` у `apply()` не утверждались вовсе (ревью A·2).
    const bool ok_clears = first_ok && g.error().empty();
    const bool second_ok = first_ok && g.apply(w, g_raw, dt_raw);
    const uint64_t second_fuel = g.fuel_used();
    const bool trap_named = first_ok && !second_ok && g.error().rfind("gravity call:", 0) == 0;
    // Третий заход: хозяин выключен трапом и уходит по `!ready_`, не дойдя до бака. Ноль здесь —
    // вторая половина контракта `fuel_used()` («измерения нет»), и до этой строки её не
    // спрашивал никто: убери обнуление из `apply()`, и гейт оставался зелёным (ревью A·2).
    const bool no_call = !g.apply(w, g_raw, dt_raw);
    const bool no_measure = no_call && g.fuel_used() == 0;

    WasmGravity small;
    const bool small_up = small.init_wat(SMALL_WAT);
    const bool small_refused = small_up && !small.apply(w, g_raw, dt_raw);
    const bool small_named = small_refused && small.error() == "wasm memory too small";
    // Отказ не выключил хозяина: второй заход отвечает тем же, а не молчанием мёртвого объекта.
    const bool small_alive = small_named && !small.apply(w, g_raw, dt_raw)
                             && small.error() == "wasm memory too small" && small.fuel_used() == 0;

    // Бак, выпитый до дна: расход равен баку ровно тогда, когда гостя остановило топливо.
    WasmGravity spin;
    const bool spin_up = spin.init_wat(SPIN_WAT);
    const bool spin_trapped = spin_up && !spin.apply(w, g_raw, dt_raw);
    const bool tank_drained = spin_trapped && spin.fuel_used() == WASM_FUEL_PER_CALL;

    // Модуль встал, экспорта с таким именем у него нет. Под одним `LinkError` на двоих этот исход
    // неотличим от отбитого инстанцирования — то есть опечатка в имени красила бы гейт побега.
    std::string why;
    const WasmOutcome noexport = wasm_run_escape(TWICE_WAT, "nope", WasmFuel::On, &why);

    const bool fuel_own = first_ok && first_fuel > 0 && !second_ok && second_fuel > first_fuel;

    std::printf("[plugin-wasm-host] the reason of a failed init survives to error(): %s\n",
                (no_file && silent && kept) ? "YES" : "NO");
    std::printf("[plugin-wasm-host] a failed init can be retried, no false 'already initialized': %s\n",
                (bad_wat && retried) ? "YES" : "NO");
    std::printf("[plugin-wasm-host] a successful init clears the previous reason: %s\n",
                err_cleared ? "YES" : "NO");
    std::printf("[plugin-wasm-host] a trapped call names its OWN spend (%llu, good call %llu): %s\n",
                static_cast<unsigned long long>(second_fuel),
                static_cast<unsigned long long>(first_fuel), fuel_own ? "YES" : "NO");
    std::printf("[plugin-wasm-host] an endless guest drains the tank (%llu of %llu): %s\n",
                static_cast<unsigned long long>(spin.fuel_used()),
                static_cast<unsigned long long>(WASM_FUEL_PER_CALL), tank_drained ? "YES" : "NO");
    std::printf("[plugin-wasm-host] a good call clears the reason, a trapped one names itself: %s\n",
                (ok_clears && trap_named) ? "YES" : "NO");
    std::printf("[plugin-wasm-host] a call that never happened measures no fuel (%llu): %s\n",
                static_cast<unsigned long long>(g.fuel_used()), no_measure ? "YES" : "NO");
    std::printf("[plugin-wasm-host] a refusal that kills nobody names itself twice: %s\n",
                small_alive ? "YES" : "NO");
    std::printf("[plugin-wasm-host] a missing export is told apart from a refused instantiation: %s\n",
                noexport == WasmOutcome::NoExport ? "YES" : "NO");
    std::printf("[plugin-wasm-host] the refusal names itself: %s\n", why.c_str());

    const bool pass = no_file && silent && kept && bad_wat && retried && err_cleared
                      && fuel_own && tank_drained && ok_clears && trap_named && no_measure
                      && small_alive
                      && (noexport == WasmOutcome::NoExport) && !why.empty();
    std::printf("plugin-wasm-host: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
