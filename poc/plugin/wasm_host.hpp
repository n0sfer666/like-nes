#pragma once
#include "sim.hpp"
#include <string>

enum class WasmOutcome { Ok, CompileError, LinkError, TrapOnCall };

class WasmGravity {
public:
    WasmGravity() = default;
    ~WasmGravity();
    WasmGravity(const WasmGravity&) = delete;
    WasmGravity& operator=(const WasmGravity&) = delete;

    bool init(const std::string& wat_path);
    bool apply(SimWorld& w, int32_t g_raw, int32_t dt_raw);
    const std::string& error() const { return err_; }

private:
    struct Impl;
    Impl* p_ = nullptr;
    bool ready_ = false;
    std::string err_;
};

WasmOutcome wasm_run_escape(const std::string& wat, const char* export_to_call);
