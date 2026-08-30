#include "instance_stage.hpp"

namespace game {

void InstanceStage::push(const Instance& inst) {
    if (count_ >= capacity_) { ++dropped_; return; }
    buf_[count_++] = inst;
}

} // namespace game
