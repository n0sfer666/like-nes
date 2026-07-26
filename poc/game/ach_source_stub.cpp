#include "ach_source.hpp"

namespace game {

struct AchSource::Impl {};

AchSource::AchSource() : impl_(new Impl()) {}

AchSource::~AchSource() = default;

bool AchSource::open(ach::Registry&, const std::string&) { return false; }

const char* AchSource::reason() const { return "bundle io unavailable on this platform"; }

} // namespace game
