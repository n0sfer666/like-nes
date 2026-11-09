#pragma once

#include <cstdio>

namespace platform::test {

inline std::FILE* temporary_file() {
#ifdef _MSC_VER
    std::FILE* file = nullptr;
    return ::tmpfile_s(&file) == 0 ? file : nullptr;
#else
    return std::tmpfile();
#endif
}

} // namespace platform::test
