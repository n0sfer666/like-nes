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

- (CGPoint)firePoint:(CGRect)frame safe:(NSArray<NSString*>*)safe {
    NSString* path = [[NSBundle bundleForClass:self.class] pathForResource:@"touch" ofType:@"txt"];
    NSError* error = nil;
    NSString* text = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&error];
    XCTAssertNotNil(text, @"cannot read the independent touch.txt oracle: %@", error);
    for (NSString* line in [text componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet]) {
        NSString* code = [line componentsSeparatedByString:@"#"].firstObject;
        NSMutableArray<NSString*>* f = [NSMutableArray new];
        for (NSString* part in [code componentsSeparatedByString:@"|"])
            [f addObject:[part stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet]];
        if (f.count != 6 || ![f[0] isEqual:@"button"] || ![f[1] isEqual:@"a"]) continue;
        XCTAssertEqualObjects(f[2], @"bottom-right", @"extend the oracle for a different anchor");
        const CGFloat radius = f[5].doubleValue;
        XCTAssertGreaterThan(radius, 1);
        CGFloat x = CGRectGetMaxX(frame) - safe[2].doubleValue - f[3].doubleValue;
        CGFloat y = CGRectGetMaxY(frame) - safe[3].doubleValue - f[4].doubleValue;
        // Inward edge: omitting the safe inset must put this finger outside the real circle.
        if (safe[2].doubleValue > 2) x -= radius - 1;
        else {
            XCTAssertGreaterThan(safe[3].doubleValue, 2);
            y -= radius - 1;
        }
        return CGPointMake(x, y);
    }
    XCTFail(@"touch.txt has no button a");
    return CGPointZero;
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

    XCUIElement* game = app.otherElements[@"game"];
    XCTAssertTrue([game waitForExistenceWithTimeout:10], @"the probe view exposes no safe area");
    for (NSNumber* deviceOrientation in @[@(UIDeviceOrientationLandscapeLeft), @(UIDeviceOrientationLandscapeRight)]) {
        const UIInterfaceOrientation expected = deviceOrientation.integerValue == UIDeviceOrientationLandscapeLeft
            ? UIInterfaceOrientationLandscapeRight : UIInterfaceOrientationLandscapeLeft;
        XCUIDevice.sharedDevice.orientation = deviceOrientation.integerValue;
        NSPredicate* settled = [NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary* bindings) {
            (void)object; (void)bindings;
            NSArray<NSString*>* values = [game.value componentsSeparatedByString:@" "];
            return values.count == 5 && values[4].integerValue == expected &&
                MAX(values[0].doubleValue, values[2].doubleValue) > 0 &&
                window.frame.size.width > window.frame.size.height;
        }];
        XCTNSPredicateExpectation* rotated = [[XCTNSPredicateExpectation alloc] initWithPredicate:settled object:game];
        XCTAssertEqual([XCTWaiter waitForExpectations:@[rotated] timeout:15], XCTWaiterResultCompleted,
                       @"landscape safe area did not settle: %@", game.value);
        [NSThread sleepForTimeInterval:0.5];
        NSArray<NSString*>* safe = [game.value componentsSeparatedByString:@" "];
        XCTAssertEqual(safe.count, 5u);
        const UIInterfaceOrientation orientation = safe[4].integerValue;
        XCTAssertEqual(orientation, expected);
        [self attachScreen:[NSString stringWithFormat:@"before-%ld", (long)orientation]];
        const CGRect f = window.frame;
        const CGPoint fire = [self firePoint:f safe:safe];
        const CGPoint stick = CGPointMake(f.origin.x + 0.25 * f.size.width, f.origin.y + 0.5 * f.size.height);

        id<LNPointerPath> stickPath = [[pathClass alloc] initForTouchAtPoint:stick offset:0.0];
        [stickPath moveToPoint:CGPointMake(stick.x + 30, stick.y) atOffset:0.3];
        [stickPath moveToPoint:CGPointMake(stick.x + 50, stick.y - 20) atOffset:0.6];
        [stickPath liftUpAtOffset:1.8];
        id<LNPointerPath> firePath = [[pathClass alloc] initForTouchAtPoint:fire offset:0.8];
        [firePath liftUpAtOffset:1.3];

        id<LNEventRecord> record = [[recordClass alloc] initWithName:@"stick+fire"
                                                interfaceOrientation:orientation];
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
        [self attachScreen:[NSString stringWithFormat:@"after-%ld", (long)orientation]];
    }
}

@end
