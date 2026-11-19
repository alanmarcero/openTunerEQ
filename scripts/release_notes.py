"""Write a GitHub release body for one tag.

Notes come from a CHANGELOG section for that version, or from the annotated
tag message when the changelog has no section. The screenshot and compare
link stay under the notes.
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

HEADING = re.compile(r"^(#{1,6})\s+(\S.*?)\s*$")
VERSION_HEADING = re.compile(
    r"^\[?v?(\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?)\]?(\s|$|[-–—:])"
)


def version_of(tag):
    return tag[1:] if tag.startswith("v") else tag


def heading_version(title):
    match = VERSION_HEADING.match(title.strip())
    return match.group(1) if match else None


def extract_changelog_section(text, version):
    """Return the body of the changelog heading for `version`, without the heading."""
    lines = text.splitlines()
    start = None
    level = None
    for index, line in enumerate(lines):
        match = HEADING.match(line)
        if not match or heading_version(match.group(2)) != version:
            continue
        start = index + 1
        level = len(match.group(1))
        break
    if start is None:
        return ""
    end = len(lines)
    for index in range(start, len(lines)):
        match = HEADING.match(lines[index])
        if match and len(match.group(1)) <= level:
            end = index
            break
    return "\n".join(lines[start:end]).strip()


def find_changelog(explicit):
    if explicit:
        path = Path(explicit)
        return path if path.is_file() else None
    for name in ("CHANGELOG.md", "CHANGELOG"):
        path = Path(name)
        if path.is_file():
            return path
    return None


def strip_signature(message):
    marker = "\n-----BEGIN PGP SIGNATURE-----"
    index = message.find(marker)
    if index != -1:
        message = message[:index]
    return message.strip()


def notes_from_tag_message(message, tag):
    """Use an annotated tag message. A version title alone is not notes."""
    message = strip_signature(message)
    if not message:
        return ""
    lines = message.splitlines()
    subject = lines[0].strip()
    body = "\n".join(lines[1:]).strip()
    version = re.escape(version_of(tag))
    title = re.fullmatch(rf"(?i)openTunerEQ\s+v?{version}|v?{version}", subject)
    if title:
        return body
    return message


def read_tag_message(tag):
    kind = subprocess.check_output(
        ["git", "for-each-ref", f"refs/tags/{tag}", "--format=%(objecttype)"],
        text=True,
    ).strip()
    if kind != "tag":
        return ""
    return subprocess.check_output(
        ["git", "for-each-ref", f"refs/tags/{tag}", "--format=%(contents)"],
        text=True,
    )


def previous_tag(tag):
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--abbrev=0", f"{tag}^"],
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except subprocess.CalledProcessError:
        return ""


def repository_name():
    configured = os.environ.get("GITHUB_REPOSITORY", "").strip()
    if configured:
        return configured
    try:
        url = subprocess.check_output(["git", "remote", "get-url", "origin"], text=True, stderr=subprocess.DEVNULL).strip()
    except subprocess.CalledProcessError:
        raise SystemExit("Set GITHUB_REPOSITORY or add an origin remote so the release links are complete.")
    match = re.search(r"[:/]([^/:]+/[^/]+?)(?:\.git)?$", url)
    if not match:
        raise SystemExit(f"Cannot tell the GitHub repository from {url}")
    return match.group(1)


def render_body(notes, tag, previous, repository):
    screenshot = (
        f"![Native editor](https://raw.githubusercontent.com/{repository}/{tag}/docs/screenshots/preview.png)"
    )
    parts = [notes.strip(), "", screenshot]
    if previous:
        parts += ["", f"**Full Changelog**: https://github.com/{repository}/compare/{previous}...{tag}"]
    return "\n".join(parts) + "\n"


def release_notes(tag, changelog_path):
    notes = ""
    changelog = find_changelog(changelog_path)
    if changelog is not None:
        notes = extract_changelog_section(changelog.read_text(encoding="utf-8"), version_of(tag))
    if not notes:
        notes = notes_from_tag_message(read_tag_message(tag), tag)
    if not notes:
        version = version_of(tag)
        raise SystemExit(
            f"No release notes for {tag}. Add a CHANGELOG.md section headed "
            f"'## [{version}]' or '## v{version}', or annotate the tag with a message."
        )
    return render_body(notes, tag, previous_tag(tag), repository_name())


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--changelog", default="")
    args = parser.parse_args(argv)
    Path(args.output).write_text(release_notes(args.tag, args.changelog), encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1:])
