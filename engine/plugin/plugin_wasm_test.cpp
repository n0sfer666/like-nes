#include "wasm_host.hpp"
#include "wasm_escape.hpp"
#include "registry.hpp"
#include "builtin.hpp"
#include "host.hpp"
#include "sim.hpp"
#include "platform_args.hpp"
#include <cstdio>
#include <string>

static const char* OOB_WAT =
    "(module (memory (export \"mem\") 1)"
    " (func (export \"boom\") (i32.store (i32.const 100000000) (i32.const 1))))";

static const char* CAP_WAT =
    "(module (import \"host\" \"secret\" (func $s))"
    " (func (export \"go\") (call $s)))";

// Время хоста: цикл без выхода. Без топлива он не побег, а вечный кадр — трапа нет, есть висящий
// поток, и ни один ассерт про память его не увидит (аудит #21, A·2·2).
static const char* LOOP_WAT =
    "(module (func (export \"spin\") (loop $l (br $l))))";

// RAM хоста: рост НА ОДНУ СТРАНИЦУ сверх потолка. В C-API v26 нет trap_on_grow_failure, отказ виден
// гостю как -1 — модуль сам обязан на него упасть, иначе тихо поедет с невыросшей памятью. Пара
// стоит на самом краю (256 страниц = 16 МиБ при стартовой одной: рост на 255 разрешён, на 256 —
// нет): окно пошире утверждает лишь «потолок где-то есть», и подмена 16 МиБ на 60 прошла бы молча.
static const char* GROW_WAT =
    "(module (memory (export \"mem\") 1)"
    " (func (export \"grow\")"
    " (if (i32.eq (memory.grow (i32.const 256)) (i32.const -1)) (then (unreachable)))))";

// Позитивный контроль того же потолка: рост РОВНО В потолок разрешён. Без него гейт выше зелен и
// тогда, когда сломан сам memory.grow, а не лимитер, — и он же ловит потолок, ставший меньше.
static const char* GROW_OK_WAT =
    "(module (memory (export \"mem\") 1)"
    " (func (export \"grow\")"
    " (if (i32.eq (memory.grow (i32.const 255)) (i32.const -1)) (then (unreachable)))))";

// Тот же потолок линейной памяти, но забранный ОБЪЯВЛЕНИЕМ, а не ростом. `GROW_WAT` выше требует
// СОТРУДНИЧЕСТВА гостя: отказ виден ему как -1, и падает он сам. Объявленные 300 страниц забирают
// RAM хоста до первой инструкции и без капли топлива — ровно тот путь, который для таблиц закрыт
// `TABLE_WAT`, и без этой строки главный довод соседнего комментария для памяти не утверждён вовсе.
// Отбивает ЛИМИТЕР, отказом инстанцирования (проба на v26: «memory minimum size of 300 pages
// exceeds memory limits»); позитивный контроль того же потолка — рост ровно в него, `GROW_OK_WAT`.
static const char* MEMORY_BIG_WAT =
    "(module (memory 300) (func (export \"go\")))";

// RAM хоста в обход потолка памяти: таблица на элемент сверх потолка. `WASM_MEMORY_LIMIT` меряет
// ТОЛЬКО линейную память, а таблица считается элементами и без своего числа не ограничена вовсе;
// объявлена она в самом модуле, поэтому гость забирает эту память, не выполнив ни одной инструкции
// и не потратив ни капли топлива. Число держится вплотную к потолку: далёкое wasmtime отбивает САМ
// на компиляции — с `(table 1000000000)` придёт `CompileError`, то есть чужой предел, а не наш.
static const char* TABLE_WAT =
    "(module (table 65537 funcref) (func (export \"go\")))";

// Позитивный контроль того же потолка: таблица РОВНО В потолок создаётся. Без него гейт выше
// зелен и тогда, когда таблицы запрещены вообще, а плагину они нужны для косвенных вызовов.
static const char* TABLE_OK_WAT =
    "(module (table 65536 funcref) (func (export \"go\")))";

// Два оставшихся числа лимитера — сколько таблиц и сколько памятей. Без фикстуры каждое молча
// вернулось бы к умолчанию в 10 000 штук, и потолки выше обходились бы счётом: десять тысяч таблиц
// по 65 536 элементов — уже не 16 МиБ. Оба отказа приходят от ЛИМИТЕРА, с отказом инстанцирования,
// а не от валидации (проба на v26: оба — `LinkError`; многопамятность включена). Позитивный
// контроль свой у каждого: одна таблица — `TABLE_OK_WAT`, одна память — игровой путь.
static const char* TABLES_WAT =
    "(module (table 1 funcref) (table 1 funcref) (func (export \"go\")))";
static const char* MEMORIES_WAT =
    "(module (memory 1) (memory 1) (func (export \"go\")))";

// Секция `(start)` выполняется на ИНСТАНЦИРОВАНИИ — до первого `set_fuel` у вызывающего. Пока бак
// наливался перед вызовом, честный старт трапал на пустом баке и приезжал `LinkError`, под именем
// «просил чужой импорт» (проба на wasmtime v26). Пара стоит на обоих исходах: конечный старт ОБЯЗАН
// инстанцироваться, бесконечный — быть отбит, иначе заправка вернула бы вечный кадр (ревью A·2).
static const char* START_WAT =
    "(module (func $s (nop)) (start $s) (func (export \"go\")))";
static const char* START_SPIN_WAT =
    "(module (func $s (loop $l (br $l))) (start $s) (func (export \"go\")))";

// Отказ ВЫЗОВА, а не гостя: у функции параметр, а зовут её без аргументов. Гость не выполнил ни
// одной инструкции — wasmtime возвращает ошибку хозяину (`wasmtime_error_t`, «expected 1 arguments,
// got 0»), трапа нет. Под общим `TrapOnCall` дрейф сигнатуры ЛЮБОЙ фикстуры таблицы оставлял бы
// свою строку зелёной, не проверив ничего: побег «отбит» словами о вызове, которого не было.
static const char* SIGNATURE_WAT =
    "(module (func (export \"go\") (param i32)))";

// `HostError` от «заправка не удалась» достижим штатным API: на песочнице БЕЗ `consume_fuel`
// `set_fuel` отвечает «fuel is not configured». Модуль тривиален нарочно: сломайся классификация,
// прогон ответит `Ok`, а не повиснет.
static const char* NOFUEL_WAT =
    "(module (func (export \"go\")))";

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr, "usage: plugin_wasm_test <gravity.wat> <wind.so>\n");
        return 2;
    }
    const std::string wat = argv[1];
    const std::string wind = argv[2];
    const int TICKS = 3000;
    const uint64_t GOLDEN = 0x7d9a6e60cbed4156ull;

    Registry reg;
    PluginHost host(reg);
    reg.set_current_owner("builtin");
    reg.add_ecs_system("integrate", {"gravity", "wind"}, sys_integrate);
    reg.add_ecs_system("bounce", {"integrate"}, sys_bounce);
    reg.set_current_owner("");
    if (!host.load_native(wind)) { std::fprintf(stderr, "cannot load wind\n"); return 1; }
    bool sok = true;
    auto sched = reg.schedule(&sok);

    WasmGravity g;
    if (!g.init(wat)) { std::fprintf(stderr, "wasm init: %s\n", g.error().c_str()); return 1; }

    const int32_t g_raw = fix32::from_float(9.8).raw;
    const int32_t dt_raw = fix32::from_float(1.0 / 60.0).raw;

    SimWorld w;
    sim_init(w);
    for (int t = 0; t < TICKS; ++t) {
        if (!g.apply(w, g_raw, dt_raw)) { std::fprintf(stderr, "wasm apply: %s\n", g.error().c_str()); return 1; }
        for (const auto& s : sched) s.fn(&w);
        w.tick++;
    }
    uint64_t h_wasm = sim_hash(w);

    const uint64_t fuel = g.fuel_used();

    // Четырнадцать строк ОДНОЙ таблицей: у каждого своё ожидание рядом с фикстурой, и причина от
    // wasmtime печатается ровно у красной строки. Пять разных отказов приезжают одним `LinkError`,
    // и без текста фикстура зеленела бы на причине соседки (аудит #21, ревью A·2).
    struct Escape { const char* label; const char* wat; const char* fn; WasmFuel fuel; WasmOutcome want; };
    static const Escape ESCAPES[] = {
        {"escape OOB linear-memory -> trap (host alive)", OOB_WAT, "boom", WasmFuel::On, WasmOutcome::TrapOnCall},
        {"escape ungranted host import -> link rejected", CAP_WAT, "go", WasmFuel::On, WasmOutcome::LinkError},
        {"escape endless loop -> out of fuel -> trap", LOOP_WAT, "spin", WasmFuel::On, WasmOutcome::TrapOnCall},
        {"escape memory.grow one page past the memory limit -> refused", GROW_WAT, "grow", WasmFuel::On, WasmOutcome::TrapOnCall},
        {"control: memory.grow exactly to the limit still grows", GROW_OK_WAT, "grow", WasmFuel::On, WasmOutcome::Ok},
        {"escape a declared memory past the memory limit -> refused", MEMORY_BIG_WAT, "go", WasmFuel::On, WasmOutcome::LinkError},
        {"escape table one element past the table limit -> refused", TABLE_WAT, "go", WasmFuel::On, WasmOutcome::LinkError},
        {"control: a table exactly at the limit still instantiates", TABLE_OK_WAT, "go", WasmFuel::On, WasmOutcome::Ok},
        {"escape a second table -> refused", TABLES_WAT, "go", WasmFuel::On, WasmOutcome::LinkError},
        {"escape a second memory -> refused", MEMORIES_WAT, "go", WasmFuel::On, WasmOutcome::LinkError},
        {"control: a module with a finite (start) instantiates", START_WAT, "go", WasmFuel::On, WasmOutcome::Ok},
        {"escape endless (start) at instantiation -> refused", START_SPIN_WAT, "go", WasmFuel::On, WasmOutcome::LinkError},
        {"a store without a fuel counter -> host error, not guest", NOFUEL_WAT, "go", WasmFuel::Off, WasmOutcome::HostError},
        {"a call the host got wrong -> host error, not a guest trap", SIGNATURE_WAT, "go", WasmFuel::On, WasmOutcome::HostError},
    };

    std::printf("[plugin-wasm] native golden = 0x%016llx\n", static_cast<unsigned long long>(GOLDEN));
    std::printf("[plugin-wasm] WASM golden   = 0x%016llx\n", static_cast<unsigned long long>(h_wasm));
    std::printf("[plugin-wasm] native == WASM (bit-exact fix32 across ABI): %s\n",
                h_wasm == GOLDEN ? "YES" : "NO");
    std::printf("[plugin-wasm] limits: memory %lld bytes, table %lld elements, fuel %llu per call\n",
                static_cast<long long>(WASM_MEMORY_LIMIT), static_cast<long long>(WASM_TABLE_LIMIT),
                static_cast<unsigned long long>(WASM_FUEL_PER_CALL));
    std::printf("[plugin-wasm] fuel per call: %llu of %llu spent by the gravity tick\n",
                static_cast<unsigned long long>(fuel), static_cast<unsigned long long>(WASM_FUEL_PER_CALL));

    bool escapes_ok = true;
    for (const Escape& esc : ESCAPES) {
        std::string why;
        const bool ok = wasm_run_escape(esc.wat, esc.fn, esc.fuel, &why) == esc.want;
        std::printf("[plugin-wasm] %s: %s\n", esc.label, ok ? "YES" : "NO");
        if (!ok && !why.empty()) std::printf("[plugin-wasm]   cause: %s\n", why.c_str());
        escapes_ok = escapes_ok && ok;
    }

    bool pass = sok && (h_wasm == GOLDEN) && escapes_ok
                // Порог — процент бака, а не сам бак: `wasmtime_context_get_fuel` документирован
                // сам себе противоречиво («amount of fuel remaining» против «fuel consumed so
                // far»), и ассерт `< WASM_FUEL_PER_CALL` проходит при ЛЮБОМ из двух смыслов.
                // Проба на v26 назвала остаток: трап после двух инструкций оставляет бак без двух.
                // Честный тик гравитации тратит около 9500 — процент от бака в десять миллионов.
                && (fuel > 0 && fuel < WASM_FUEL_PER_CALL / 100);
    std::printf("plugin-wasm: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
