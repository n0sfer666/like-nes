#include "wasm_host.hpp"
#include "wasm_sandbox.hpp"
#include "wasm_error.hpp"
#include "wasm_extern.hpp"
#include <cstring>
#include <new>

#include "platform_fs.hpp"

struct WasmGravity::Impl {
    wasm_engine_t* engine = nullptr;
    wasmtime_store_t* store = nullptr;
    wasmtime_context_t* ctx = nullptr;
    wasmtime_module_t* module = nullptr;
    wasmtime_instance_t instance{};
    wasmtime_memory_t memory{};
    wasmtime_func_t gravity{};

    // Освобождение живёт здесь: `init()` бросает недостроенный Impl, а вторая копия разошлась бы.
    ~Impl() {
        if (module) wasmtime_module_delete(module);
        if (store) wasmtime_store_delete(store);
        if (engine) wasm_engine_delete(engine);
    }
};

WasmGravity::~WasmGravity() { delete p_; }
// `p_ = nullptr` между двумя шагами: бросься `new` — деструктор пришёл бы на освобождённый Impl.
void WasmGravity::reset_impl() { delete p_; p_ = nullptr; p_ = new Impl(); }

// Impl создаётся ДО чтения файла, а вход сторожит `ready_`, а не `p_`. Политика одна на ВСЕ семь
// отказов `init()`: Impl остаётся жив, `apply()` уходит по `!ready_` молча, и причина доживает до
// `error()` — освободи его здесь, и «cannot read <путь>» затёрлось бы словами «not initialized».
// Сторож по `p_` объявлял бы «already initialized» объекту, у которого init ПРОВАЛИЛСЯ (ревью A·2).
//
// Восьмой отказ — исчерпание памяти в `new Impl()` или в строке причины: он один уходил бы
// ИСКЛЮЧЕНИЕМ мимо `false` и мимо `error()`, то есть политика «все отказы отвечают одинаково»
// держалась бы на семи ветках из восьми. Функциональный try-блок ловит его обоим входам; текст
// причины короче SSO, поэтому сам обработчик не аллоцирует (ревью A·2).
bool WasmGravity::init(const std::string& wat_path) try {
    if (ready_) { err_ = "already initialized"; return false; }
    reset_impl();
    std::string wat;
    if (!platform::read_text(wat_path, wat)) { err_ = "cannot read " + wat_path; return false; }
    return init_module(wat);
} catch (const std::bad_alloc&) { err_ = "out of memory"; return false; }

bool WasmGravity::init_wat(const std::string& wat) try {
    if (ready_) { err_ = "already initialized"; return false; }
    reset_impl();
    return init_module(wat);
} catch (const std::bad_alloc&) { err_ = "out of memory"; return false; }

bool WasmGravity::init_module(const std::string& wat) {
    if (!make_sandbox(p_->engine, p_->store, p_->ctx, WasmFuel::On)) {
        err_ = "cannot build sandbox"; return false;
    }

    wasm_byte_vec_t wasm;
    wasmtime_error_t* e = wasmtime_wat2wasm(wat.data(), wat.size(), &wasm);
    if (e) { err_ = "wat2wasm: " + take_error(e, nullptr); return false; }

    e = wasmtime_module_new(p_->engine, reinterpret_cast<uint8_t*>(wasm.data), wasm.size, &p_->module);
    wasm_byte_vec_delete(&wasm);
    if (e) { err_ = "module_new: " + take_error(e, nullptr); return false; }

    wasm_trap_t* trap = nullptr;
    e = wasmtime_instance_new(p_->ctx, p_->module, nullptr, 0, &p_->instance, &trap);
    if (e || trap) { err_ = "instance_new: " + take_error(e, trap); return false; }

    // `wasmtime_extern_t` из API — собственность вызывающего (`extern.h`): значение копируется в
    // Impl, а сам extern освобождает сторож на выходе из блока — на КАЖДОМ из трёх (ревью A·2).
    {
        WasmExtern item;
        if (!wasmtime_instance_export_get(p_->ctx, &p_->instance, "mem", 3, &item.v)) {
            err_ = "no exported memory 'mem'"; return false;
        }
        if (item.v.kind != WASMTIME_EXTERN_MEMORY) { err_ = "export 'mem' is not a memory"; return false; }
        p_->memory = item.v.of.memory;
    }
    {
        WasmExtern item;
        if (!wasmtime_instance_export_get(p_->ctx, &p_->instance, "gravity", 7, &item.v)) {
            err_ = "no exported func 'gravity'"; return false;
        }
        if (item.v.kind != WASMTIME_EXTERN_FUNC) { err_ = "export 'gravity' is not a func"; return false; }
        p_->gravity = item.v.of.func;
    }

    err_.clear();
    ready_ = true;
    return true;
}

// Расход читается и ПОСЛЕ трапа: `wasmtime_context_get_fuel` на трапнувшем store отвечает штатно, и
// гость, ушедший в цикл, называет весь бак, а упавший на второй инструкции — двойку (проба, v26).
void WasmGravity::read_fuel() {
    uint64_t left = 0;
    wasmtime_error_t* e = wasmtime_context_get_fuel(p_->ctx, &left);
    if (e) { wasmtime_error_delete(e); return; }
    fuel_used_ = WASM_FUEL_PER_CALL - left;
}

bool WasmGravity::apply(SimWorld& w, int32_t g_raw, int32_t dt_raw) {
    // Расход обнуляется ПЕРВЫМ действием: инвариант «`fuel_used()` — расход ПОСЛЕДНЕГО вызова»
    // держится конструкцией, а не тем, что все ранние выходы случайно стоят выше (ревью A·2).
    fuel_used_ = 0;
    // Причину выключения назвал тот вызов, который её увидел. Перезапись `err_` словом «not
    // initialized» стёрла бы улику там, где её читают: нечего сообщать только до `init()` (ревью A·2).
    if (p_ == nullptr) { err_ = "not initialized"; return false; }
    if (!ready_) return false;
    const size_t bytes = SimWorld::N * sizeof(int32_t);
    if (wasmtime_memory_data_size(p_->ctx, &p_->memory) < bytes) { err_ = "wasm memory too small"; return false; }

    uint8_t* mem = wasmtime_memory_data(p_->ctx, &p_->memory);
    for (int i = 0; i < SimWorld::N; ++i) {
        int32_t v = w.vy[i].raw;
        std::memcpy(mem + i * sizeof(int32_t), &v, sizeof(int32_t));
    }

    wasmtime_val_t args[4];
    args[0].kind = WASMTIME_I32; args[0].of.i32 = 0;
    args[1].kind = WASMTIME_I32; args[1].of.i32 = SimWorld::N;
    args[2].kind = WASMTIME_I32; args[2].of.i32 = g_raw;
    args[3].kind = WASMTIME_I32; args[3].of.i32 = dt_raw;

    // Топливо выдаётся на один вызов: гость, ушедший в цикл, выбирает его и получает трап вместо
    // повисшего кадра. Бак не налился — мерить нечего, `fuel_used()` остаётся нулём.
    wasmtime_error_t* e = wasmtime_context_set_fuel(p_->ctx, WASM_FUEL_PER_CALL);
    if (e) { err_ = "set_fuel: " + take_error(e, nullptr); ready_ = false; return false; }

    wasm_trap_t* trap = nullptr;
    e = wasmtime_func_call(p_->ctx, &p_->gravity, args, 4, nullptr, 0, &trap);
    // Трапнувший вызов топливо ПОТРАТИЛ, и сколько — единственная улика «кадр съело топливо»:
    // ноль на этом месте не отличим от «вызова не было» (аудит #21, ревью A·2).
    if (e || trap) { err_ = "gravity call: " + take_error(e, trap); read_fuel(); ready_ = false; return false; }
    read_fuel();

    mem = wasmtime_memory_data(p_->ctx, &p_->memory);
    for (int i = 0; i < SimWorld::N; ++i) {
        int32_t v;
        std::memcpy(&v, mem + i * sizeof(int32_t), sizeof(int32_t));
        w.vy[i].raw = v;
    }
    err_.clear();
    return true;
}
