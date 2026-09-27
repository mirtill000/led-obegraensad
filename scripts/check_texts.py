# PlatformIO pre-build script: keeps the names the user sees consistent.
# Fails the build if a retired name comes back in anything shown on the
# page or the panel (web/, the string literals in src/ and include/, the
# README), or if two modes/animations get names that differ only by a
# trailing "s" (like "Space Invader" and "Space Invaders").
import os
import re
import sys

Import("env")  # noqa: F821 (provided by PlatformIO)

root = env["PROJECT_DIR"]

# Retired name -> what it is called now.
RETIRED = {
    "Attuale (": "Media (8 px)",
    "Mini 3×5": "Piccola (5 px)",
    "Mini 3x5": "Piccola (5 px)",
    "Piccolo 3 righe": "Minima (4 px)",
    "font 4 pixel": "the font's name (Minima)",
    "Ripristina quelle predefinite": "Cancella le mie frasi",
}


def files(folder, exts):
    for base, _, names in os.walk(os.path.join(root, folder)):
        for n in names:
            if n.endswith(exts):
                yield os.path.join(base, n)


def literals(text):
    return re.findall(r'"((?:[^"\\\n]|\\.)*)"', text)


problems = []
sources = [(p, open(p, encoding="utf-8").read()) for p in files("web", (".html", ".js", ".css"))]
sources.append((os.path.join(root, "README.md"), open(os.path.join(root, "README.md"), encoding="utf-8").read()))
code = [(p, " ".join(literals(open(p, encoding="utf-8").read())))
        for p in list(files("src", (".cpp", ".h"))) + list(files("include", (".h",)))
        if not p.endswith(("webpage.h", "quotes_builtin.h", "build_info.h"))]
for path, text in sources + code:
    for old, new in RETIRED.items():
        if old in text:
            problems.append(f"{os.path.relpath(path, root)}: '{old}' is now '{new}'")

names = set()
for path, _ in code:
    text = open(path, encoding="utf-8").read()
    names.update(re.findall(r'name\(\) const override \{ return "([^"]+)"', text))
for n in names:
    if n + "s" in names:
        problems.append(f"names too alike: '{n}' and '{n}s'")

if problems:
    print("\n*** Inconsistent names shown to the user (scripts/check_texts.py):")
    for p in problems:
        print("  - " + p)
    sys.exit(1)
