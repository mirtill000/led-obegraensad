# PlatformIO pre-build script: keeps the texts the user sees consistent.
# Fails the build if
#  - a retired name comes back in anything shown on the page or the panel
#    (web/, the string literals in src/ and include/, the README);
#  - two modes/animations get names that differ only by a trailing "s"
#    (like "Space Invader" and "Space Invaders");
#  - a message of include/texts.h is written out again in src/ instead of
#    using txt:: (the same situation must read the same everywhere, and a
#    translation must only touch texts.h);
#  - the page lists by itself the choices of a setting: their values and
#    names come from the lamp (SETTING_DEFS, the state's "choices").
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

# Messages that belong to texts.h.
texts = open(os.path.join(root, "include", "texts.h"), encoding="utf-8").read()
messages = dict((v, k) for k, v in re.findall(r'constexpr const char \*(\w+) = "((?:[^"\\]|\\.)*)";', texts))
for path in list(files("src", (".cpp", ".h"))):
    for lit in literals(open(path, encoding="utf-8").read()):
        if len(lit) > 6 and lit in messages:
            problems.append(f"{os.path.relpath(path, root)}: \"{lit}\" - use txt::{messages[lit]} (include/texts.h)")

# Setting choices are the lamp's: the page's lists for them stay empty.
page = open(os.path.join(root, "web", "page.html"), encoding="utf-8").read()
app = open(os.path.join(root, "web", "app.js"), encoding="utf-8").read()
selects = re.search(r"CHOICE_SELECTS = \{([^}]*)\}", app)
for sel in re.findall(r"(\w+):", selects.group(1) if selects else ""):
    if re.search(r'<select id="%s"[^>]*>\s*<option' % sel, page):
        problems.append(f"web/page.html: the list '{sel}' has its own options - they come from the lamp (SETTING_DEFS)")

if problems:
    print("\n*** Inconsistent names shown to the user (scripts/check_texts.py):")
    for p in problems:
        print("  - " + p)
    sys.exit(1)
