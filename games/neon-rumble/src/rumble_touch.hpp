#pragma once
#include <string>

#include "bundle_view.hpp"
#include "touch_source.hpp"

namespace rumble {

// Данные раскладки из бандла; координаты платформы переводятся в fix32 в MobileRumble.
class Touch {
public:
    bool load(asset::BundleView bundle, std::string& why);
    framework::touch::TouchSource& source() { return source_; }
    const framework::touch::TouchSource& source() const { return source_; }
    std::string state() const;
    void reveal() { revealed_ = true; }
    void update_pads(uint32_t connected) {
        if ((connected & ~pads_) != 0) revealed_ = false;
        pads_ = connected;
    }
    bool visible(bool owns_seat) const { return owns_seat || pads_ == 0 || revealed_; }

private:
    framework::touch::TouchSource source_;
    bool revealed_ = false;
    uint32_t pads_ = 0;
};

} // namespace rumble
