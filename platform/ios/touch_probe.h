#pragma once
#import <UIKit/UIKit.h>

#include <cstdint>
#include <string>

#include "mobile_app.hpp"

// Шов гейта симулятора (`scripts/ios_sim_gate.sh`), живой только под флагом `--touch-probe`.
void probe_expose_safe_area(UIView* view);
void probe_log(intptr_t id, mobile::Touch phase, CGPoint p, const std::string& state, NSString* run);
