#!/usr/bin/env python3
"""Mechanical UI-refresh gates for production QML."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
QML_ROOT = ROOT / "src" / "app" / "qml"
THEME = QML_ROOT / "Theme.qml"
HEX_COLOR = re.compile(r"#[0-9A-Fa-f]{3,8}\b")
LITERAL_DURATION = re.compile(r"\bduration\s*:\s*\d+\b")


def matches(path: Path, pattern: re.Pattern[str]):
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if pattern.search(line):
            yield number, line.strip()


def main() -> int:
    failures: list[str] = []
    for path in sorted(QML_ROOT.rglob("*.qml")):
        if path != THEME:
            for number, line in matches(path, HEX_COLOR):
                failures.append(f"{path.relative_to(ROOT)}:{number}: literal color: {line}")
        for number, line in matches(path, LITERAL_DURATION):
            failures.append(f"{path.relative_to(ROOT)}:{number}: literal animation duration: {line}")

    if failures:
        print("\n".join(failures))
        print(f"\n{len(failures)} QML style gate failure(s)")
        return 1

    print("Production QML uses centralized color and motion tokens.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
