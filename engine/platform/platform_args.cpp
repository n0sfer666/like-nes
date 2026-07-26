#include "platform_args.hpp"

namespace platform {

void Args::adopt(int argc, char** argv) {
    owned_.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) owned_.emplace_back(argv[i]);
}

void Args::index(int& argc, char**& argv) {
    ptrs_.reserve(owned_.size() + 1);
    for (std::string& s : owned_) ptrs_.push_back(s.data());
    ptrs_.push_back(nullptr);
    argc = static_cast<int>(owned_.size());
    argv = ptrs_.data();
}

} // namespace platform
