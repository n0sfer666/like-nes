#import <XCTest/XCTest.h>

// Публичный XCUITest кладёт два пальца только рядом с центром элемента (twoFingerTap, pinch), а
// гейту нужны стик слева и огонь в круге справа внизу одновременно. Произвольные точки даёт только
// приватный синтез XCUIAutomation; его пропажа в новом Xcode — провал гейта с именем класса, а не
// откат на публичный жест, который зон не различает.
@protocol LNPointerPath <NSObject>
- (instancetype)initForTouchAtPoint:(CGPoint)point offset:(double)offset;
- (void)moveToPoint:(CGPoint)point atOffset:(double)offset;
- (void)liftUpAtOffset:(double)offset;
@end
@protocol LNEventRecord <NSObject>
- (instancetype)initWithName:(NSString*)name interfaceOrientation:(UIInterfaceOrientation)orientation;
- (void)addPointerEventPath:(id)path;
@end
@protocol LNEventSynthesizer <NSObject>
- (void)synthesizeEvent:(id)record completion:(void (^)(BOOL ok, NSError* error))completion;
@end

@interface TouchGateTests : XCTestCase
@end

@implementation TouchGateTests

- (void)setUp {
    self.continueAfterFailure = NO;
}

- (void)attachScreen:(NSString*)name {
    XCTAttachment* a = [XCTAttachment attachmentWithScreenshot:XCUIScreen.mainScreen.screenshot];
    a.name = name;
    a.lifetime = XCTAttachmentLifetimeKeepAlways;
    [self addAttachment:a];
}

- (Class)privateClass:(NSString*)name selectors:(NSArray<NSString*>*)sels {
    Class c = NSClassFromString(name);
    XCTAssertNotNil(c, @"private %@ is gone from this Xcode: the gate cannot place two fingers", name);
    for (NSString* s in sels)
        XCTAssertTrue([c instancesRespondToSelector:NSSelectorFromString(s)], @"%@ lost -%@", name, s);
    return c;
}

- (void)testStickAndFireTogether {
    Class pathClass = [self privateClass:@"XCPointerEventPath"
                               selectors:@[@"initForTouchAtPoint:offset:", @"moveToPoint:atOffset:", @"liftUpAtOffset:"]];
    Class recordClass = [self privateClass:@"XCSynthesizedEventRecord"
                                 selectors:@[@"initWithName:interfaceOrientation:", @"addPointerEventPath:"]];
    XCTAssertTrue([XCUIDevice.sharedDevice respondsToSelector:NSSelectorFromString(@"eventSynthesizer")],
                  @"private XCUIDevice.eventSynthesizer is gone from this Xcode: the gate cannot place two fingers");
    id<LNEventSynthesizer> synth = [XCUIDevice.sharedDevice valueForKey:@"eventSynthesizer"];
    XCTAssertTrue([synth respondsToSelector:@selector(synthesizeEvent:completion:)], @"eventSynthesizer lost -synthesizeEvent:completion:");

    // Нонс прогона кладёт гейт (TEST_RUNNER_LIKE_NES_TOUCH_RUN): по нему вердикт отличает строки
    // этого запуска от прошлых в той же выборке лога.
    NSString* run = NSProcessInfo.processInfo.environment[@"LIKE_NES_TOUCH_RUN"];
    XCTAssertNotNil(run, @"LIKE_NES_TOUCH_RUN is not set: run this test through scripts/ios_sim_gate.sh");
    XCUIApplication* app = [XCUIApplication new];
    app.launchArguments = @[@"--touch-probe"];
    app.launchEnvironment = @{@"LIKE_NES_TOUCH_RUN": run};
    [app launch];
    XCUIElement* window = app.windows.firstMatch;
    XCTAssertTrue([window waitForExistenceWithTimeout:30], @"app window never appeared");
    [NSThread sleepForTimeInterval:2.0];
    [self attachScreen:@"launch"];

    // Геометрия — независимый оракул `fire_btn` из mobile_game.cpp, а не его копия по смыслу:
    // уедет кнопка — палец мимо круга, и гейт скажет «огонь не взят», а не пройдёт.
    const CGRect f = window.frame;
    const CGFloat w = f.size.width, h = f.size.height, s = MIN(w, h);
    const CGFloat r = 0.13 * s, m = 0.045 * s;
    const CGPoint fire = CGPointMake(f.origin.x + w - m - r, f.origin.y + h - m - r);
    const CGPoint stick = CGPointMake(f.origin.x + 0.25 * w, f.origin.y + 0.5 * h);

    id<LNPointerPath> stickPath = [[pathClass alloc] initForTouchAtPoint:stick offset:0.0];
    [stickPath moveToPoint:CGPointMake(stick.x + 30, stick.y) atOffset:0.3];
    [stickPath moveToPoint:CGPointMake(stick.x + 50, stick.y - 20) atOffset:0.6];
    [stickPath liftUpAtOffset:1.8];
    id<LNPointerPath> firePath = [[pathClass alloc] initForTouchAtPoint:fire offset:0.8];
    [firePath liftUpAtOffset:1.3];

    id<LNEventRecord> record = [[recordClass alloc] initWithName:@"stick+fire"
                                            interfaceOrientation:UIInterfaceOrientationPortrait];
    [record addPointerEventPath:stickPath];
    [record addPointerEventPath:firePath];

    XCTestExpectation* done = [self expectationWithDescription:@"two-finger gesture delivered"];
    __block NSError* failure = nil;
    [synth synthesizeEvent:record completion:^(BOOL ok, NSError* error) {
        if (!ok) failure = error ?: [NSError errorWithDomain:@"like-nes" code:1 userInfo:nil];
        [done fulfill];
    }];
    [self waitForExpectations:@[done] timeout:15];
    XCTAssertNil(failure, @"gesture synthesis failed: %@", failure);
    // Светлый кадр — это и домашний экран: упавшая после жеста игра прошла бы проверку скриншота.
    XCTAssertEqual(app.state, XCUIApplicationStateRunningForeground, @"the game did not survive the gesture");
    [self attachScreen:@"after-gesture"];
}

@end
