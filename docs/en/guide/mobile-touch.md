# Touch controls on a phone

English · [Русский](../../ru/guide/mobile-touch.md)

This is the physical follow-up of spec #26 B2e. The owner's immediate acceptance criterion is a
live emulator where screen controls move the character and trigger attacks without demo input;
the phone's ergonomics remain a separate open check. CPU tests and injected emulator fingers judge routing;
your hands judge reach, accidental presses and whether the controls cover the fight. The gate is
[§38 of owner verification](../../owner-verification.md#38-gate-2-of-26--the-touch-layout-on-a-phone).
It remains open until somebody plays level 1 on a real phone and records the result.

## Start interactive Neon Rumble

The runnable procedure below uses an Android 10+ phone with `arm64-v8a`, USB debugging accepted
on the phone, and Android SDK Platform Tools (`adb`) in PATH. Run from the repository root.
Disconnect other adb devices and stop emulators: the existing phone build script requires exactly
one device. See [owner setup, R.5a](../../owner-setup.txt) for SDK/NDK installation and expected
build output. The first build takes minutes.

```nushell
bash scripts/check_sdk_game.sh --keep
bash scripts/android_rumble.sh --abi arm64-v8a
```

That script deliberately launches `--demo` for desktop hash parity. Stop it and launch a fresh
interactive process with a unique probe nonce. Replace `PHONE_SERIAL` with the physical phone's
serial shown by `adb devices -l`; keep it for every command below.

```nushell
let serial = 'PHONE_SERIAL'
let run = $"owner-touch-(random uuid)"
adb -s $serial shell am force-stop com.likenes.neonrumble
adb -s $serial shell am start -W -n com.likenes.neonrumble/android.app.NativeActivity --es probe $run --es args --wave
adb -s $serial logcat -v raw -s like-nes | save $"touch-($run).log"
```

Leave logcat running while playing; Ctrl+C finishes capture. The new process prints
`[game] controls touch` and `[game] level mode wave`. `[safe]` and `[touch]` rows carry
`run=<your nonce>`; older rows do not belong to this check. The `args` string extra passes generic
launch flags to the game. `--wave` enables the existing waves, checkpoints and stage camera;
without it the app opens the training arena on the level 1 map, which cannot prove progression.
The app enters the selected mode directly. On an already signed iPhone
build, use Xcode scheme arguments `--wave --touch-probe`, no `--demo`, and set the scheme's
`LIKE_NES_TOUCH_RUN` environment variable to your unique nonce. Use the same hand steps below;
device signing/deployment are manual, and the Android commands do not deploy to iOS.

## Play with both thumbs

1. Tap PUNCH to claim P1, then lift. The lobby reports `P1 joining`, then `P1 present`. The first
   tap may only join; subsequent taps are attacks. With no finger down the fighter must not walk
   or keep attacking on its own.
2. Put the left thumb anywhere on the left half. The floating stick appears under it. Walk
   left/right and in depth, then lift: movement stops. A touch on the right outside a button does
   nothing. The stick may follow a thumb dragged beyond its radius.
3. Keep the stick held and use the right thumb on each button. Lift only the right thumb: the
   left thumb must keep moving the fighter. Do ten deliberate taps on each button and note
   missed taps or neighbouring buttons taken by mistake.

| Label | Action to observe |
|---|---|
| PUNCH | Jab; repeated timed hits can chain into cross. |
| KICK | Kick on the ground. |
| JUMP | Jump; tap PUNCH in the air for a jump kick. |
| GRAB | A successful grab throws the nearby enemy in the same action. A miss out of range is expected. |
| BLOCK | Block while held, leave block on release. |

4. Hold BLOCK, bring the stick to neutral, then flick it sharply outward: DODGE rolls in the
   fighter's facing direction. A slow drag or a flick without BLOCK must not dodge. This uses
   the input-frame window, not wall-clock gesture timing.
5. Hold the stick and PUNCH, rotate 180° to the other landscape position, and lift both thumbs.
   The old holds must be cancelled. Start a new gesture: the stick and all five labelled buttons
   work again, stay clear of the cutout and system gesture edge, and are upright. Repeat steps
   2–4 on this side. UIKit may report equal left/right safe insets; this alone is not a defect.
6. With the stick and a button held, go Home for five seconds and return; repeat with the power
   button. Old holds must not resume. New gestures work, and a surface detach/attach pair keeps
   the same `tick` and `world`. This checks a surviving process; saves across process death are
   a later milestone.
7. Play through level 1. Record where you stopped if you cannot finish. Judge whether every
   button is reachable without changing grip, labels are readable and the thumbs/buttons leave
   enough of the fight visible. A layout that works in logs but hurts or causes accidental
   actions has not passed this gate.

## Record the result

Send the commit, phone model, OS version and screen size/density; screenshots of both landscape
positions; your nonce-scoped log; missed/accidental presses per button; the point reached in level
1; and the change you would make if a control is uncomfortable. State pass or the concrete
problem. A simulator pass, a build pass and the scripted demo do not close this physical gate.

The layout lives in `games/neon-rumble/controls/touch.txt`: offsets and radii are logical points
from safe-area corners, not framebuffer pixels. A layout change requires rebuilding the bundle,
updating its expected hash and rerunning touch/SDK checks. Record the phone result separately;
the current criterion before B3 is the live emulator movement/attack check described above.

## Gamepads and haptic feedback

A native gamepad can claim a free mobile seat with a button. Its axes move that fighter;
unplugging pauses the fight until a device is reclaimed. Touch can occupy the other seat.
A kept local hit gives one pulse per player per tick: the attacker's device gets a light pulse,
the hurt player's a stronger pulse. Only a touch-owned seat uses the phone motor. Scripted demo
and replay checks do not send pulses.

The SDK exports `framework_haptic` and `haptic.hpp`: `haptic::pulse({haptic::Kind::Phone}, 128, 30)`
or `haptic::pulse({haptic::Kind::Pad, slot}, 128, 30)`. Strength is a byte (0–255), duration is
`uint16_t` milliseconds; zero strength or duration cancels. The mobile shell owns initialization,
pause and shutdown. Desktop callers own that lifecycle. iOS uses a transient tap, whose duration
is set by the motor; Android and gamepads use the requested duration. An unavailable phone motor
logs `[haptic] phone unavailable (no-op)` once at startup and ignores pulses.

On hardware, try a hit using touch and a hit using the pad, then background and return during
feedback. The assigned device should respond, output should stop in the background and new hits
should work after return. A simulator or modeled JNI test cannot confirm a real motor.
## Background persistence

`mobile::Game::suspend` pauses activity; persistent writes belong in the separate synchronous
`save` callback. iOS invokes it under a background task before releasing the surface. Android
uses `APP_CMD_SAVE_STATE`, whose native glue acknowledges completion after the handler returns;
`APP_CMD_STOP` repeats the attempt without that OS wait guarantee. `PAUSE` only pauses and
cancels held input. The shell sets the save directory before game initialization: Application
Support on iOS and internal application data on Android.

The shooter uses this callback for achievements. If its directory becomes available late, it
preserves the old file, adds lifetime counters with saturation and unions unlocks once. Corrupt
or unknown snapshots remain intact. Neon Rumble currently has no persistent settings/progress
to write; its in-memory match is not serialized by this hook. Local host tests cover the real
callback code with modeled OS events; a live background/kill check remains a separate device test.
