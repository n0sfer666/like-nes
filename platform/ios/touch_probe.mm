#import "touch_probe.h"

// Отступы safe area — тесту гейта: центр кнопки он считает сам, из touch.txt и этих чисел UIKit.
void probe_expose_safe_area(UIView* view) {
    const UIEdgeInsets in = view.safeAreaInsets;
    view.isAccessibilityElement = YES;
    view.accessibilityIdentifier = @"game";
    view.accessibilityValue = [NSString stringWithFormat:@"%g %g %g %g", in.left, in.top, in.right, in.bottom];
}

// Строка на касание с состоянием ПОСЛЕ него: XCUITest проигрывает жест целиком, и гейт судит историю
// из системного лога. id32 — усечение до int, каким id был до B8: улика, что оно не склеивает
// касания. run — нонс прогона: чужие строки вердикт отбрасывает.
void probe_log(intptr_t id, mobile::Touch phase, CGPoint p, const std::string& state, NSString* run) {
    static const char* const kPhase[] = {"down", "move", "up"};
    NSLog(@"[touch] %s id=%ld id32=%d x=%.0f y=%.0f %s run=%@", kPhase[static_cast<int>(phase)],
          static_cast<long>(id), static_cast<int32_t>(id), p.x, p.y, state.c_str(), run);
}
