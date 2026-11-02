#pragma once
#include <string_view>

#include "section_open.hpp"
#include "strings_format.hpp"

namespace framework::ui {
uint64_t string_id(std::string_view key);
bool string_key_ok(std::string_view key);

// Zero-copy, zero allocation; views borrow the aligned, unmodified buffer for their lifetime.
class Strings {
public:
    bool open(const void* data, size_t size);
    uint32_t count() const { return view_.header ? view_.header->count : 0; }
    uint32_t fallbacks() const { return meta_ ? meta_->fallbacks : 0; }
    const char* font_name() const { return meta_ ? view_.strings + meta_->font : ""; }
    const char* key(uint32_t index) const;
    std::string_view find(uint64_t id, Language language = Language::En) const;
    std::string_view find(std::string_view key, Language language = Language::En) const;
private:
    const StringRow* find_row(uint64_t id) const;
    core::SectionView view_;
    const StringRow* rows_ = nullptr;
    const StringsMeta* meta_ = nullptr;
};
}
