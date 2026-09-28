#include "wasm_sandbox.hpp"

// Инстанс один, память одна, таблица одна: отрицательное число оставило бы умолчание, а умолчания
// тут разные — размер памяти и число элементов таблицы не ограничены ВОВСЕ, а инстансов, таблиц и
// памятей по умолчанию 10 000 ШТУК. Бак наливается ЗДЕСЬ, а не перед первым вызовом: store с пустым
// баком трапает на инстанцировании, и честный `(start)` приезжал бы `LinkError` (проба, wasmtime v26).
bool make_sandbox(wasm_engine_t*& engine, wasmtime_store_t*& store,
                  wasmtime_context_t*& ctx, WasmFuel fuel) {
    wasm_config_t* cfg = wasm_config_new();
    if (cfg == nullptr) return false;
    wasmtime_config_consume_fuel_set(cfg, fuel == WasmFuel::On);
    engine = wasm_engine_new_with_config(cfg);   // движок забирает конфиг себе
    if (engine == nullptr) return false;
    store = wasmtime_store_new(engine, nullptr, nullptr);
    if (store == nullptr) return false;
    wasmtime_store_limiter(store, WASM_MEMORY_LIMIT, WASM_TABLE_LIMIT, 1, 1, 1);
    ctx = wasmtime_store_context(store);
    if (fuel == WasmFuel::Off) return true;
    wasmtime_error_t* e = wasmtime_context_set_fuel(ctx, WASM_FUEL_PER_CALL);
    if (e) { wasmtime_error_delete(e); return false; }
    return true;
}
