#include "wasm_error.hpp"

std::string take_error(wasmtime_error_t* e, wasm_trap_t* t) {
    wasm_byte_vec_t msg;
    std::string out;
    if (e) { wasmtime_error_message(e, &msg); out.assign(msg.data, msg.size); wasm_byte_vec_delete(&msg); wasmtime_error_delete(e); }
    if (t) { wasm_trap_message(t, &msg); if (out.empty()) out.assign(msg.data, msg.size); wasm_byte_vec_delete(&msg); wasm_trap_delete(t); }
    return out;
}
