#include "platform_module.hpp"

#include <dlfcn.h>

#include <string>

namespace platform {
namespace {

// dlerror() ПОТРЕБЛЯЕТ сообщение: второй вызов подряд отдал бы «unknown error», а вызывающий,
// который сначала логирует, а потом сверяет, получил бы разные тексты об одной ошибке. Держим
// копию сами, как это делает win32-двойник, — last_error() становится чистым чтением.
thread_local std::string g_error;

} // namespace

bool Module::open(const std::string& utf8_path) {
    close();
    dlerror(); // сбросить чужую висящую ошибку, иначе она станет «нашей» при неудаче
    // RTLD_LOCAL: символы плагина не попадают в глобальную область — два плагина с одинаковым
    // именем функции не переопределяют друг друга молча.
    handle_ = dlopen(utf8_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle_ == nullptr) {
        const char* e = dlerror();
        g_error = e ? e : "dlopen failed: " + utf8_path;
    }
    return handle_ != nullptr;
}

void Module::close() {
    if (handle_) dlclose(handle_);
    handle_ = nullptr;
}

void* Module::symbol(const char* name) const {
    if (!handle_) return nullptr;
    dlerror();
    void* sym = dlsym(handle_, name);
    if (sym == nullptr) {
        const char* e = dlerror();
        g_error = e ? e : std::string("symbol not found: ") + name;
    }
    return sym;
}

const char* Module::last_error() {
    return g_error.empty() ? "unknown error" : g_error.c_str();
}

} // namespace platform
