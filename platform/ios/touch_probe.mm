#import "touch_probe.h"

#include "mobile_probe.hpp"

// Отступы safe area — тесту гейта: центр кнопки он считает сам, из touch.txt и этих чисел UIKit.
void probe_expose_safe_area(UIView* view, NSString* run) {
    const UIEdgeInsets in = view.safeAreaInsets;
    view.isAccessibilityElement = YES;
    view.accessibilityIdentifier = @"game";
    view.accessibilityValue = [NSString stringWithFormat:@"%g %g %g %g %ld", in.left, in.top, in.right,
        in.bottom, static_cast<long>(view.window.windowScene.interfaceOrientation)];
    const float scale = static_cast<float>(view.traitCollection.displayScale);
    const mobile::SafeArea safe{static_cast<float>(in.left), static_cast<float>(in.top),
        static_cast<float>(in.right), static_cast<float>(in.bottom), scale};
    const UIInterfaceOrientation o = view.window.windowScene.interfaceOrientation;
    const int rotation = o == UIInterfaceOrientationLandscapeLeft ? 1 :
        o == UIInterfaceOrientationLandscapeRight ? 3 : o == UIInterfaceOrientationPortraitUpsideDown ? 2 : 0;
    NSLog(@"%s", mobile::safe_line(static_cast<uint32_t>(view.bounds.size.width * scale),
        static_cast<uint32_t>(view.bounds.size.height * scale), safe, rotation, run.UTF8String).c_str());
}

// Строка на касание с состоянием ПОСЛЕ него: XCUITest проигрывает жест целиком, и гейт судит историю
// из системного лога. id32 — усечение до int, каким id был до B8: улика, что оно не склеивает
// касания. run — нонс прогона: чужие строки вердикт отбрасывает.
void probe_log(intptr_t id, mobile::Touch phase, CGPoint p, const std::string& state, NSString* run) {
    NSLog(@"%s", mobile::touch_line(id, phase, static_cast<float>(p.x), static_cast<float>(p.y), state,
                                   run.UTF8String).c_str());
}
