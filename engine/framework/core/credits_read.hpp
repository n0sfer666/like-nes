#pragma once
#include <cstddef>
#include <cstdint>

#include "credits_format.hpp"
#include "section_open.hpp"

namespace framework::core {

struct Credit {
    const char* pack = "";
    const char* author = "";
    const char* license = "";
    const char* url = "";
    const char* attribution = "";
};

class CreditTable {
public:
    bool open(const void* data, std::size_t size);
    bool valid() const { return view_.header != nullptr; }
    uint32_t count() const { return valid() ? view_.header->count : 0; }
    Credit at(uint32_t index) const;

private:
    SectionView view_;
    const CreditRow* rows_ = nullptr;
};

} // namespace framework::core
