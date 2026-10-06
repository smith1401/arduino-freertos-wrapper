#!/usr/bin/env python3
"""Version helper for the library.

The version lives in library.json (PlatformIO), library.properties (Arduino
IDE) and as a "## X.Y.Z" heading in CHANGELOG.md. This script keeps them in
sync and is used by the release workflow.

    scripts/version.py show             print the current version
    scripts/version.py check [VERSION]  verify all files agree (and match VERSION)
    scripts/version.py set VERSION      update all files to VERSION
    scripts/version.py notes VERSION    print the CHANGELOG section of VERSION
"""

import datetime
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIBRARY_JSON = ROOT / "library.json"
LIBRARY_PROPERTIES = ROOT / "library.properties"
CHANGELOG = ROOT / "CHANGELOG.md"

SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z.-]+)?$")
# "## 2.0.0", "## 2.0.0 - 2026-10-06", "## [2.0.0] - 2026-10-06"
HEADING = re.compile(r"^## \[?(?P<version>[^\]\s]+)\]?(?:\s+-\s+.*)?\s*$")
PROPERTY = re.compile(r"^version=(?P<version>.*)$", re.MULTILINE)


def fail(message):
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)


def normalize(version):
    """Accept tags like v2.0.0 as well as plain versions."""
    version = version.strip()
    if version.startswith("v"):
        version = version[1:]
    if not SEMVER.match(version):
        fail(f"'{version}' is not a semantic version (X.Y.Z)")
    return version


def json_version():
    return json.loads(LIBRARY_JSON.read_text())["version"]


def properties_version():
    match = PROPERTY.search(LIBRARY_PROPERTIES.read_text())
    if not match:
        fail(f"no version= line in {LIBRARY_PROPERTIES.name}")
    return match.group("version").strip()


def changelog_sections():
    """Map version -> section text (without the heading)."""
    sections = {}
    current = None
    for line in CHANGELOG.read_text().splitlines():
        match = HEADING.match(line)
        if match:
            current = match.group("version")
            sections[current] = []
        elif current is not None:
            sections[current].append(line)
    return {v: "\n".join(lines).strip() + "\n" for v, lines in sections.items()}


def cmd_show():
    print(json_version())


def cmd_check(expected=None):
    versions = {
        LIBRARY_JSON.name: json_version(),
        LIBRARY_PROPERTIES.name: properties_version(),
    }
    problems = []

    if len(set(versions.values())) != 1:
        problems.append("version mismatch: " + ", ".join(f"{k}={v}" for k, v in versions.items()))

    version = versions[LIBRARY_JSON.name]
    if expected is not None and normalize(expected) != version:
        problems.append(f"files are at {version}, expected {normalize(expected)}")

    sections = changelog_sections()
    if version not in sections:
        problems.append(f"{CHANGELOG.name} has no '## {version}' section")
    elif not sections[version].strip():
        problems.append(f"the {CHANGELOG.name} section for {version} is empty")

    if problems:
        fail("; ".join(problems))
    print(f"ok: version {version}")


def cmd_set(version):
    version = normalize(version)

    data = LIBRARY_JSON.read_text()
    data, count = re.subn(r'("version"\s*:\s*")[^"]*(")', rf"\g<1>{version}\g<2>", data, count=1)
    if count != 1:
        fail(f"no version field in {LIBRARY_JSON.name}")
    LIBRARY_JSON.write_text(data)

    props = LIBRARY_PROPERTIES.read_text()
    LIBRARY_PROPERTIES.write_text(PROPERTY.sub(f"version={version}", props, count=1))

    # Turn "## Unreleased" into the new version, or add an empty section
    changelog = CHANGELOG.read_text()
    if version not in changelog_sections():
        today = datetime.date.today().isoformat()
        unreleased = re.compile(r"^## \[?Unreleased\]?\s*$", re.MULTILINE | re.IGNORECASE)
        if unreleased.search(changelog):
            changelog = unreleased.sub(f"## {version} - {today}", changelog, count=1)
        else:
            changelog = re.sub(r"^(# .*\n)", rf"\g<1>\n## {version} - {today}\n\n", changelog, count=1)
        CHANGELOG.write_text(changelog)

    print(f"version set to {version}, now describe the changes in {CHANGELOG.name}")


def cmd_notes(version):
    version = normalize(version)
    sections = changelog_sections()
    if version not in sections:
        fail(f"{CHANGELOG.name} has no '## {version}' section")
    sys.stdout.write(sections[version])


def main(argv):
    commands = {
        "show": (cmd_show, 0, 0),
        "check": (cmd_check, 0, 1),
        "set": (cmd_set, 1, 1),
        "notes": (cmd_notes, 1, 1),
    }
    if len(argv) < 2 or argv[1] not in commands:
        print(__doc__.strip(), file=sys.stderr)
        sys.exit(2)

    func, min_args, max_args = commands[argv[1]]
    args = argv[2:]
    if not min_args <= len(args) <= max_args:
        print(__doc__.strip(), file=sys.stderr)
        sys.exit(2)
    func(*args)


if __name__ == "__main__":
    main(sys.argv)
