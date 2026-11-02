#import <CoreHaptics/CoreHaptics.h>

#include <atomic>

#include "platform_haptic.hpp"

namespace haptic::phone {
namespace {
CHHapticEngine* engine;
id<CHHapticPatternPlayer> player;
std::atomic<bool> reset{false};
bool warned = false;

void report(NSError* error) {
    if (error && !warned) { NSLog(@"[haptic] phone error %@", error); warned = true; }
}
}

bool init() {
    if (engine) return true;
    if (!CHHapticEngine.capabilitiesForHardware.supportsHaptics) return false;
    NSError* error = nil;
    engine = [[CHHapticEngine alloc] initAndReturnError:&error];
    if (!engine || error) { engine = nil; return false; }
    engine.playsHapticsOnly = YES;
    // Core Haptics зовёт handler на своём потоке. Объекты меняет лишь игровой/UI-поток.
    engine.resetHandler = ^{ reset.store(true); };
    if (![engine startAndReturnError:&error]) { engine.resetHandler = ^{}; engine = nil; return false; }
    warned = false;
    return true;
}
void cancel() {
    if (!engine) return;
    [player stopAtTime:CHHapticTimeImmediate error:nil];
    player = nil;
    [engine stopWithCompletionHandler:nil];
}
void pulse(float strength, int ms) {
    if (!engine) return;
    if (strength <= 0 || ms <= 0) { cancel(); return; }
    if (reset.exchange(false)) player = nil; // старые players после reset недействительны
    NSError* error = nil;
    // Также восстанавливает движок после фоновой паузы, auto-stop или прерывания аудиосессии.
    if (![engine startAndReturnError:&error]) { report(error); return; }
    [player stopAtTime:CHHapticTimeImmediate error:nil];
    CHHapticEventParameter* intensity = [[CHHapticEventParameter alloc]
        initWithParameterID:CHHapticEventParameterIDHapticIntensity value:strength];
    CHHapticEventParameter* sharpness = [[CHHapticEventParameter alloc]
        initWithParameterID:CHHapticEventParameterIDHapticSharpness value:.5f];
    // У transient длительность задаёт мотор iOS: ms управляет продолжительностью Android/пада.
    CHHapticEvent* event = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticTransient
        parameters:@[intensity, sharpness] relativeTime:0];
    CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
    if (!pattern || error) { report(error); return; }
    player = [engine createPlayerWithPattern:pattern error:&error];
    if (player && !error) [player startAtTime:CHHapticTimeImmediate error:&error];
    report(error);
}
void shutdown() {
    cancel();
    engine.resetHandler = ^{};
    engine = nil;
    reset = false;
}
} // namespace haptic::phone
