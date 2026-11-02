#!/usr/bin/env python3
"""iOS entry point for the shared landscape touch verdict; the shooter owns its layout."""

import sys
from pathlib import Path

from touch_verdict import main

if __name__ == "__main__":
    args = sys.argv
    if len(args) == 3:
        args = [*args, str(Path(__file__).resolve().parent.parent /
                          "example_ugly_game/assets/touch.txt")]
    # UIKit safe insets protect rounded corners too; both landscape sides may be equal.
    sys.exit(main(args, cutout_swap=False))
