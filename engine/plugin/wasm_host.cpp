#include "wasm_host.hpp"
#include <wasmtime.h>
#include <cstring>
#include <sstream>
#include <vector>

#include "platform_fs.hpp"

static std::string take_error(wasmtime_error_t* e, wasm_trap_t* t) {
    wasm_byte_vec_t msg;
    std::string out;
    if (e) { wasmtime_error_message(e, &msg); out.assign(msg.data, msg.size); wasm_byte_vec_delete(&msg); wasmtime_error_delete(e); }
    if (t) { wasm_trap_message(t, &msg); if (out.empty()) out.assign(msg.data, msg.size); wasm_byte_vec_delete(&msg); wasm_trap_delete(t); }
    return out;
}

static bool read_file(const std::string& path, std::string& out) {
    return platform::read_text(path, out);
}

struct WasmGravity::Impl {
    wasm_engine_t* engine = nullptr;
    wasmtime_store_t* store = nullptr;
    wasmtime_context_t* ctx = nullptr;
    wasmtime_module_t* module = nullptr;
    wasmtime_instance_t instance{};
    wasmtime_memory_t memory{};
    wasmtime_func_t gravity{};
};

WasmGravity::~WasmGravity() {
    if (!p_) return;
    if (p_->module) wasmtime_module_delete(p_->module);
    if (p_->store) wasmtime_store_delete(p_->store);
    if (p_->engine) wasm_engine_delete(p_->engine);
    delete p_;
}

bool WasmGravity::init(const std::string& wat_path) {
    if (p_) { err_ = "already initialized"; return false; }
    std::string wat;
    if (!read_file(wat_path, wat)) { err_ = "cannot read " + wat_path; return false; }

    p_ = new Impl();
    p_->engine = wasm_engine_new();
    p_->store = wasmtime_store_new(p_->engine, nullptr, nullptr);
    p_->ctx = wasmtime_store_context(p_->store);

    wasm_byte_vec_t wasm;
    wasmtime_error_t* e = wasmtime_wat2wasm(wat.data(), wat.size(), &wasm);
    if (e) { err_ = "wat2wasm: " + take_error(e, nullptr); return false; }

    e = wasmtime_module_new(p_->engine, reinterpret_cast<uint8_t*>(wasm.data), wasm.size, &p_->module);
    wasm_byte_vec_delete(&wasm);
    if (e) { err_ = "module_new: " + take_error(e, nullptr); return false; }

    wasm_trap_t* trap = nullptr;
    e = wasmtime_instance_new(p_->ctx, p_->module, nullptr, 0, &p_->instance, &trap);
    if (e || trap) { err_ = "instance_new: " + take_error(e, trap); return false; }

    wasmtime_extern_t item;
    if (!wasmtime_instance_export_get(p_->ctx, &p_->instance, "mem", 3, &item) || item.kind != WASMTIME_EXTERN_MEMORY) {
        err_ = "no exported memory 'mem'"; return false;
    }
    p_->memory = item.of.memory;
    if (!wasmtime_instance_export_get(p_->ctx, &p_->instance, "gravity", 7, &item) || item.kind != WASMTIME_EXTERN_FUNC) {
        err_ = "no exported func 'gravity'"; return false;
    }
    p_->gravity = item.of.func;
    ready_ = true;
    return true;
}

bool WasmGravity::apply(SimWorld& w, int32_t g_raw, int32_t dt_raw) {
    if (!p_ || !ready_) { err_ = "not initialized"; return false; }
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

    wasm_trap_t* trap = nullptr;
    wasmtime_error_t* e = wasmtime_func_call(p_->ctx, &p_->gravity, args, 4, nullptr, 0, &trap);
    if (e || trap) { err_ = "gravity call: " + take_error(e, trap); return false; }

    mem = wasmtime_memory_data(p_->ctx, &p_->memory);
    for (int i = 0; i < SimWorld::N; ++i) {
        int32_t v;
        std::memcpy(&v, mem + i * sizeof(int32_t), sizeof(int32_t));
        w.vy[i].raw = v;
    }
    return true;
}

WasmOutcome wasm_run_escape(const std::string& wat, const char* export_to_call) {
    wasm_engine_t* engine = wasm_engine_new();
    wasmtime_store_t* store = wasmtime_store_new(engine, nullptr, nullptr);
    wasmtime_context_t* ctx = wasmtime_store_context(store);
    WasmOutcome result = WasmOutcome::Ok;
    wasmtime_module_t* module = nullptr;

    wasm_byte_vec_t wasm;
    wasmtime_error_t* e = wasmtime_wat2wasm(wat.data(), wat.size(), &wasm);
    if (e) { take_error(e, nullptr); result = WasmOutcome::CompileError; goto done; }

    e = wasmtime_module_new(engine, reinterpret_cast<uint8_t*>(wasm.data), wasm.size, &module);
    wasm_byte_vec_delete(&wasm);
    if (e) { take_error(e, nullptr); result = WasmOutcome::CompileError; goto done; }

    {
        wasmtime_instance_t instance{};
        wasm_trap_t* trap = nullptr;
        e = wasmtime_instance_new(ctx, module, nullptr, 0, &instance, &trap);
        if (e || trap) { take_error(e, trap); result = WasmOutcome::LinkError; goto done; }

        if (export_to_call) {
            wasmtime_extern_t item;
            if (!wasmtime_instance_export_get(ctx, &instance, export_to_call, std::strlen(export_to_call), &item)
                || item.kind != WASMTIME_EXTERN_FUNC) {
                result = WasmOutcome::LinkError; goto done;
            }
            trap = nullptr;
            e = wasmtime_func_call(ctx, &item.of.func, nullptr, 0, nullptr, 0, &trap);
            if (e || trap) { take_error(e, trap); result = WasmOutcome::TrapOnCall; goto done; }
        }
    }

done:
    if (module) wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);
    return result;
}
