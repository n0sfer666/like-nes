#include "ach_source.hpp"

#include "../achievements/bundle_source.hpp"

namespace game {

struct AchSource::Impl {
    ach::BundleSource source;
    ach::SourceStatus status;
};

AchSource::AchSource() : impl_(new Impl()) {}

AchSource::~AchSource() = default;

bool AchSource::open(ach::Registry& reg, const std::string& bundle_path) {
    impl_->status = impl_->source.open(reg, bundle_path);
    return impl_->status.result == ach::SourceResult::Ok;
}

const char* AchSource::reason() const { return ach::source_reason(impl_->status); }

} // namespace game
