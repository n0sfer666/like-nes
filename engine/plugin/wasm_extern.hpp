#pragma once
#include <wasmtime.h>

// Сторож над `wasmtime_extern_t`. Значение, ВОЗВРАЩЁННОЕ из API, освобождает вызывающий
// (`extern.h`), и рукописный `wasmtime_extern_delete` держится ровно до первого выхода, который
// его обойдёт: `goto` мимо конца блока, ранний `return`, бросок. Ни один гейт такой течи не
// скажет — она молчит. Нулевая инициализация делает сторожа безопасным и ДО удачного
// `..._export_get`: kind 0 — это функция, то есть значение, а освобождать `wasmtime_extern_delete`
// умеет только разделяемую память (аудит #21, ревью A·2).
struct WasmExtern {
    wasmtime_extern_t v{};
    WasmExtern() = default;
    ~WasmExtern() { wasmtime_extern_delete(&v); }
    WasmExtern(const WasmExtern&) = delete;
    WasmExtern& operator=(const WasmExtern&) = delete;
};
