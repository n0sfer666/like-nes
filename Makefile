.DEFAULT_GOAL := help
.PHONY: help build rumble-build sidescroller rumble check types lint owner-check owner owner-import preflight \
	ci-watch ios-shooter ios-rumble android-emu android-rumble

ARGS :=
GPU :=
ABI := arm64-v8a
AVD :=

help:
	$(info Build:)
	$(info make build          configure and build build/ - sidescroller, editor, tests)
	$(info make rumble-build   SDK prefix plus Neon Rumble against it - check_sdk_game.sh --keep)
	$(info Play - each checks the binary first and warns when sources are newer:)
	$(info make sidescroller   build/game_sidescroller            ARGS=... - on Windows GPU=low/high/d3d12/vulkan instead)
	$(info make rumble         build-sdk-work/game-Release/neon_rumble   ARGS=--coop and so on)
	$(info Checks:)
	$(info make check          types and lint from .context/checks.json - the pre-commit pair)
	$(info make owner-check    scripts/owner_check.sh - report in build/owner-report-OS.txt)
	$(info make owner          Neon Rumble gates by script in a window - frame sheet, report pushed   ARGS="--no-push --no-gpu")
	$(info make owner-import   push a clean-check report of a clean box to the owner runs   ARGS=<report>)
	$(info make preflight      everything CI runs, before push)
	$(info make ci-watch       CI runs of HEAD after push)
	$(info Mobile - macOS only:)
	$(info make ios-shooter    sidescroller in the iOS simulator - builds build-ios itself)
	$(info make ios-rumble     Neon Rumble in the iOS simulator - needs make rumble-build first)
	$(info make android-emu    boot an Android AVD unless adb already sees a device   AVD=name)
	$(info make android-rumble Neon Rumble on the emulator or phone - boots the AVD if none   ABI=arm64-v8a/x86_64)
	@:

ifeq ($(OS),Windows_NT)

SHELL := cmd.exe
WIN := scripts\win-dev.bat

build:
	@$(WIN) build
rumble-build:
	@$(WIN) sdk
sidescroller:
	@$(WIN) game $(GPU)
rumble:
	@$(WIN) rumble $(ARGS)
owner-check:
	@$(WIN) check
owner:
	@$(WIN) owner $(ARGS)
check types lint owner-import preflight ci-watch:
	@echo make $@: not wired on Windows - run scripts\win-dev.bat shell, then Git Bash: bash scripts/... & exit /b 1
ios-shooter ios-rumble android-emu android-rumble:
	@echo make $@: mobile targets run on macOS only & exit /b 1

else

UNAME := $(shell uname -s)
SIDE := build/game_sidescroller
RUMBLE := build-sdk-work/game-Release/neon_rumble
RUMBLE_GRAPH := build-sdk-release:like_nes_sdk,assetc build-sdk-work/game-Release:neon_rumble
MAC_ONLY = @[ "$(UNAME)" = Darwin ] || { echo "make $@: mobile targets run on macOS only" >&2; exit 1; }

define runnable
@test -x $(1) || { echo "make: $(1) is missing - build it: make $(2)" >&2; exit 1; }
@n=$$(bash scripts/stale_bin.sh $(1) $(3)); \
	[ -z "$$n" ] || echo "make: warning: $$n changed after $(1) was built - rebuild: make $(2)" >&2
@case " $(ARGS) " in *" --headless "* | *" --demo "*) exit 0 ;; esac; \
	[ "$(UNAME)" != Linux ] || [ -n "$$DISPLAY$$WAYLAND_DISPLAY" ] \
	|| { echo "make: no desktop session - DISPLAY and WAYLAND_DISPLAY are unset" >&2; exit 1; }
endef

define same_sdk
@b=$$(sed -n 's/^set(_ln_build "\(.*\)")$$/\1/p' build-sdk-work/prefix/lib/cmake/like-nes/like-nesConfig.cmake 2>/dev/null); \
	h=$$(git rev-parse --short HEAD); \
	[ "$$(git rev-parse -q --verify "$${b##*+}^{commit}")" = "$$(git rev-parse HEAD)" ] \
	|| { echo "make: SDK prefix in build-sdk-work is $${b:-missing}, HEAD is $$h - rebuild: make rumble-build" >&2; $(1); }
endef

build:
	@[ -f build/CMakeCache.txt ] || cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build build
rumble-build:
	bash scripts/check_sdk_game.sh --keep
sidescroller:
	$(call runnable,$(SIDE),build,build:game_sidescroller)
	./$(SIDE) $(ARGS)
rumble:
	$(call runnable,$(RUMBLE),rumble-build,$(RUMBLE_GRAPH))
	$(call same_sdk,true)
	./$(RUMBLE) $(ARGS)

check: types lint
types lint:
	@cmd=$$(python3 -c 'import json, sys; print(json.load(open(".context/checks.json", encoding="utf-8"))[sys.argv[1]])' $@) \
		&& echo "== $@: $$cmd" && bash -c "$$cmd"
owner-check:
	bash scripts/owner_check.sh
owner:
	bash scripts/owner_run.sh $(ARGS)
owner-import:
	bash scripts/owner_import.sh $(ARGS)
preflight:
	bash scripts/preflight.sh
ci-watch:
	bash scripts/ci_watch.sh

ios-shooter:
	$(MAC_ONLY)
	bash scripts/ios_sim_run.sh
ios-rumble:
	$(MAC_ONLY)
	$(call runnable,$(RUMBLE),rumble-build,$(RUMBLE_GRAPH))
	$(call same_sdk,exit 1)
	bash scripts/ios_sim_rumble.sh
android-emu:
	$(MAC_ONLY)
	bash scripts/android_emu.sh $(AVD)
android-rumble:
	$(MAC_ONLY)
	$(call runnable,$(RUMBLE),rumble-build,$(RUMBLE_GRAPH))
	$(call same_sdk,exit 1)
	@$(MAKE) --no-print-directory android-emu
	bash scripts/android_rumble.sh --abi $(ABI)

endif
