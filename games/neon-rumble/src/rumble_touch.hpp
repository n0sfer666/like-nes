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

private:
    framework::touch::TouchSource source_;
};

} // namespace rumble
