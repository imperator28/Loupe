#!/usr/bin/env python3
"""WCAG contrast gate for the Indigo Precision token set (design.md).

Checks every text-role/surface pair for >= 4.5:1 and every non-text
control-boundary pair for >= 3:1, in both themes. Exits nonzero on any
failure so it can run in CI. Update the token tables here whenever
design.md changes; the two must stay identical.
"""

import sys

TEXT_MIN = 4.5
NONTEXT_MIN = 3.0

DARK = {
    "canvas.0": "#0F1116",
    "surface.1": "#151823",
    "surface.2": "#1B1F2C",
    "surface.3": "#232838",
    "border.strong": "#6C7690",
    "text.primary": "#F2F4F9",
    "text.secondary": "#B3BACD",
    "text.tertiary": "#8B93A8",
    "accent": "#8B93F8",
    "accent.vivid": "#A78BFA",
    "on.accent": "#12142B",
    "warning": "#FDB022",
    "error": "#F97066",
    "success": "#47CD89",
    "measure": "#FFD166",
}

LIGHT = {
    "canvas.0": "#F2F3F8",
    "surface.1": "#FFFFFF",
    "surface.2": "#E9EBF3",
    "surface.3": "#FFFFFF",
    "border.strong": "#7A829E",
    "text.primary": "#1A1D28",
    "text.secondary": "#4A5165",
    "text.tertiary": "#616880",
    "accent": "#4F46E5",
    "accent.vivid": "#7C3AED",
    "on.accent": "#FFFFFF",
    "warning": "#B54708",
    "error": "#B42318",
    "success": "#067647",
    "measure": "#8A5300",
}

SURFACES = ["canvas.0", "surface.1", "surface.2", "surface.3"]
TEXT_ROLES = [
    "text.primary", "text.secondary", "text.tertiary",
    "accent", "warning", "error", "success", "measure",
]
NONTEXT_ROLES = ["border.strong", "accent"]


def relative_luminance(hex_color):
    hex_color = hex_color.lstrip("#")
    channels = [int(hex_color[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    linear = [
        c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
        for c in channels
    ]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast(a, b):
    la, lb = relative_luminance(a), relative_luminance(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def check_theme(name, palette):
    failures = []
    for role in TEXT_ROLES:
        for surface in SURFACES:
            ratio = contrast(palette[role], palette[surface])
            status = "PASS" if ratio >= TEXT_MIN else "FAIL"
            if status == "FAIL":
                failures.append((role, surface, ratio, TEXT_MIN))
            print(f"  [{name}] {role} on {surface}: {ratio:.2f} (>= {TEXT_MIN}) {status}")
    for role in NONTEXT_ROLES:
        for surface in SURFACES:
            ratio = contrast(palette[role], palette[surface])
            status = "PASS" if ratio >= NONTEXT_MIN else "FAIL"
            if status == "FAIL":
                failures.append((role, surface, ratio, NONTEXT_MIN))
            print(f"  [{name}] non-text {role} on {surface}: {ratio:.2f} (>= {NONTEXT_MIN}) {status}")
    for fill in ["accent", "accent.vivid"]:
        ratio = contrast(palette["on.accent"], palette[fill])
        status = "PASS" if ratio >= TEXT_MIN else "FAIL"
        if status == "FAIL":
            failures.append(("on.accent", fill, ratio, TEXT_MIN))
        print(f"  [{name}] on.accent on {fill}: {ratio:.2f} (>= {TEXT_MIN}) {status}")
    return failures


def main():
    failures = []
    for name, palette in (("dark", DARK), ("light", LIGHT)):
        failures += check_theme(name, palette)
    if failures:
        print(f"\n{len(failures)} contrast failure(s)")
        return 1
    print("\nAll token pairs pass WCAG contrast requirements.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
