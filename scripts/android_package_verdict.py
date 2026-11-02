#!/usr/bin/env python3
"""Проверка испечённого AndroidManifest.xml и связанной темы из aapt2 dump."""
import re
import sys
from pathlib import Path

import py_utf8


def verdict(manifest: str, resources: str) -> str:
    def attribute(name: str) -> list[str]:
        return re.findall(rf"^\s*A: http://schemas.android.com/apk/res/android:{name}"
                          r"\(0x[0-9a-f]+\)=(\S+)", manifest, re.M)

    for name, want in (("screenOrientation", ["6"]), ("hasCode", ["false"])):
        if attribute(name) != want:
            return f"{name} must be {want[0]} exactly once"
    for permission in ("INTERNET", "VIBRATE"):
        if f'"android.permission.{permission}"' not in manifest:
            return f"missing {permission} permission"
    themes = attribute("theme")
    if len(themes) != 1 or not re.fullmatch(r"@0x[0-9a-f]+", themes[0]):
        return "application theme must reference one compiled resource"
    theme_id = themes[0][1:]
    styles = re.split(r"(?m)^    resource ", resources)[1:]
    matching = [s for s in styles if s.startswith(f"{theme_id} style/LikeNesTheme\n")]
    if len(matching) != 1:
        return "manifest theme is not compiled LikeNesTheme"
    # Публичные стабильные resource id Android: windowFullscreen и windowLayoutInDisplayCutoutMode.
    for field, value in (("0x0101020d", "true"), ("0x01010586", "1")):
        values = re.findall(rf"^\s*{field}=(\S+)$", matching[0], re.M)
        if values != [value]:
            return f"theme {field} must be {value} exactly once"
    return ""


def selftest() -> int:
    attr = "A: http://schemas.android.com/apk/res/android:"
    manifest = "\n".join((f"{attr}screenOrientation(0x0101001e)=6",
                          f"{attr}hasCode(0x0101000c)=false",
                          f"{attr}theme(0x01010000)=@0x7f010000",
                          '"android.permission.INTERNET"', '"android.permission.VIBRATE"'))
    resources = ("    resource 0x7f010000 style/LikeNesTheme\n"
                 "        0x0101020d=true\n        0x01010586=1\n")
    cases = [(manifest, resources, False)]
    for before, after in (("=6", "=1"), ("=false", "=true"), ("INTERNET", "OTHER"),
                          ("VIBRATE", "OTHER"), ("@0x7f010000", "@0x7f010001"),
                          ("@0x7f010000", "missing"), ("theme(", "other(")):
        cases.append((manifest.replace(before, after), resources, True))
    cases.append((manifest + "\n" + manifest, resources, True))
    for before, after in (("0x7f010000", "0x7f010001"), ("LikeNesTheme", "Other"),
                          ("=true", "=false"), ("0x01010586=1", "0x01010586=0")):
        cases.append((manifest, resources.replace(before, after), True))
    cases.append((manifest, "", True))
    for index, (m, r, bad) in enumerate(cases):
        if bool(verdict(m, r)) != bad:
            print(f"android-package: FAIL selftest {index}")
            return 1
    print(f"android-package: PASS {len(cases)} controls")
    return 0


def main() -> int:
    py_utf8.enable()
    if sys.argv[1:] == ["--selftest"]:
        return selftest()
    if len(sys.argv) != 3:
        print("usage: android_package_verdict.py <manifest.dump> <resources.dump> | --selftest")
        return 1
    why = verdict(*(Path(p).read_text(encoding="utf-8") for p in sys.argv[1:]))
    print(f"android-package: {'FAIL ' + why if why else 'PASS compiled landscape, permissions and cutout theme'}")
    return int(bool(why))


if __name__ == "__main__":
    raise SystemExit(main())
