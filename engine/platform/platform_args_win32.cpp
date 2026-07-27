#include "platform_args.hpp"

#include <windows.h>

#include <shellapi.h>

#include <cwchar>

#include "win32_utf.hpp"

namespace platform {
namespace {

// Блок CommandLineToArgvW освобождает вызывающий, а narrow()/push_back между вызовом и
// освобождением могут бросить bad_alloc — поэтому LocalFree висит на деструкторе, а не на
// строке после цикла.
struct LocalBlock {
    explicit LocalBlock(wchar_t** block) : p(block) {}
    ~LocalBlock() {
        if (p != nullptr) LocalFree(p);
    }
    LocalBlock(const LocalBlock&) = delete;
    LocalBlock& operator=(const LocalBlock&) = delete;

    wchar_t** p;
};

} // namespace

Args::Args(int& argc, char**& argv) {
    int wide_count = 0;
    LocalBlock wide{CommandLineToArgvW(GetCommandLineW(), &wide_count)};
    if (wide.p != nullptr) {
        owned_.reserve(static_cast<size_t>(wide_count));
        for (int i = 0; i < wide_count; ++i) {
            owned_.push_back(win32::narrow(wide.p[i], std::wcslen(wide.p[i])));
        }
    } else {
        // Разбор строки запуска отказал. Узкий argv испорчен на неASCII, но это всё, что есть:
        // упасть на старте хуже, чем отработать на путях, которые уцелели.
        adopt(argc, argv);
    }
    index(argc, argv);
}

} // namespace platform
