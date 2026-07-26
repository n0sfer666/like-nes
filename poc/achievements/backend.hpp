#pragma once
#include <cstdint>

namespace ach {

enum class Send : uint32_t { Ok = 0, Retry = 1, Fatal = 2 };

class Backend {
public:
    virtual ~Backend() = default;
    virtual bool begin() = 0;
    virtual void declare(const char* key) = 0;
    virtual Send unlock(const char* key) = 0;
    virtual Send set_stat(const char* key, uint64_t value) = 0;
    virtual Send commit() = 0;
    // Возвращённые ключи принадлежат бэкенду и обязаны жить до следующего вызова: доставка читает
    // их уже после возврата, копий не делает.
    virtual int32_t poll_remote(const char** out_keys, int32_t cap) = 0;
    virtual void end() = 0;
};

} // namespace ach
