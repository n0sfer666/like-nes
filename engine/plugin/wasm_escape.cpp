#include "wasm_escape.hpp"
#include "wasm_error.hpp"
#include "wasm_extern.hpp"
#include <cstring>

static void say(std::string* why, std::string text) {
    if (why) *why = std::move(text);
}

WasmOutcome wasm_run_escape(const std::string& wat, const char* export_to_call, WasmFuel fuel,
                            std::string* why) {
    wasm_engine_t* engine = nullptr;
    wasmtime_store_t* store = nullptr;
    wasmtime_context_t* ctx = nullptr;
    WasmOutcome result = WasmOutcome::Ok;
    wasmtime_module_t* module = nullptr;
    // Всё, через что перепрыгивает `goto done`, объявлено до прыжка: иначе ошибка компиляции.
    wasm_byte_vec_t wasm;
    wasmtime_error_t* e = nullptr;
    // Песочница не построилась — отказ ХОЗЯИНА, а не гостя: с `LinkError` гейт побега зеленел бы
    // там, где `wasm_config_new` вернул nullptr, не проверив ничего (аудит #21, ревью A·2).
    if (!make_sandbox(engine, store, ctx, fuel)) {
        say(why, "sandbox not built");
        result = WasmOutcome::HostError; goto done;
    }

    e = wasmtime_wat2wasm(wat.data(), wat.size(), &wasm);
    if (e) { say(why, take_error(e, nullptr)); result = WasmOutcome::CompileError; goto done; }

    e = wasmtime_module_new(engine, reinterpret_cast<uint8_t*>(wasm.data), wasm.size, &module);
    wasm_byte_vec_delete(&wasm);
    if (e) { say(why, take_error(e, nullptr)); result = WasmOutcome::CompileError; goto done; }

    {
        wasmtime_instance_t instance{};
        wasm_trap_t* trap = nullptr;
        e = wasmtime_instance_new(ctx, module, nullptr, 0, &instance, &trap);
        if (e || trap) { say(why, take_error(e, trap)); result = WasmOutcome::LinkError; goto done; }

        if (export_to_call) {
            // Владение по контракту `extern.h` держит сторож: выход отсюда есть и прыжком на
            // `done`, и рукописный delete первый же такой прыжок обошёл бы молча (ревью A·2).
            WasmExtern item;
            // Модуль встал, а функции нет — кривая фикстура, а не отбитый побег (ревью A·2).
            if (!wasmtime_instance_export_get(ctx, &instance, export_to_call, std::strlen(export_to_call), &item.v)) {
                say(why, std::string("no export '") + export_to_call + "'");
                result = WasmOutcome::NoExport; goto done;
            }
            if (item.v.kind != WASMTIME_EXTERN_FUNC) {
                say(why, std::string("export '") + export_to_call + "' is not a func");
                result = WasmOutcome::NoExport;
            } else {
                // Заправка не удалась — тоже отказ ХОЗЯИНА: `LinkError` обвинил бы гостя (ревью A·2).
                e = wasmtime_context_set_fuel(ctx, WASM_FUEL_PER_CALL);
                if (e) { say(why, take_error(e, nullptr)); result = WasmOutcome::HostError; }
                else {
                    trap = nullptr;
                    e = wasmtime_func_call(ctx, &item.v.of.func, nullptr, 0, nullptr, 0, &trap);
                    // `e` и `trap` — разные обвиняемые. Ошибку ВЫЗОВА (не та сигнатура, чужой
                    // store) возвращает хозяин, и гость к ней не притронулся: под `TrapOnCall`
                    // опечатка в параметрах фикстуры красила бы гейт побега, не проверив ничего
                    // (проба: `(func (export "go") (param i32))` даёт «expected 1 arguments, got
                    // 0») (ревью A·2).
                    if (e) { say(why, take_error(e, trap)); result = WasmOutcome::HostError; }
                    else if (trap) { say(why, take_error(nullptr, trap)); result = WasmOutcome::TrapOnCall; }
                }
            }
        }
    }

done:
    if (module) wasmtime_module_delete(module);
    if (store) wasmtime_store_delete(store);
    if (engine) wasm_engine_delete(engine);
    return result;
}
