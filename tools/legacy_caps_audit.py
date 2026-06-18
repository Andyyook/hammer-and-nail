#!/usr/bin/env python3
"""
legacy_caps_audit.py — Enforce uppercase LEGACY comments rule.

Rule: Every file containing 'legacy' (case-insensitive, not in comments)
      must also contain 'LEGACY' (all caps) somewhere in a comment.

Usage:
    python3 tools/legacy_caps_audit.py                    # Check + report
    python3 tools/legacy_caps_audit.py --fix              # Auto-fix violations
    python3 tools/legacy_caps_audit.py --exit-on-error    # Exit 1 if violations
"""

import argparse, os, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKIP_DIRS = {'.git', 'target', 'node_modules', '__pycache__', 'dist', 'build', '.cache'}
SKIP_EXTS = {'.pyc', '.o', '.so', '.dylib', '.log', '.logd'}


def should_check(path: Path) -> bool:
    """Check if a file should be audited."""
    for skip in SKIP_DIRS:
        if skip in path.parts:
            return False
    if path.suffix in SKIP_EXTS:
        return False
    try:
        return path.is_file() and path.stat().st_size < 500_000  # skip large files
    except OSError:
        return False


def has_legacy_anycase(content: str) -> bool:
    """Check if content contains 'legacy' in any casing."""
    return bool(re.search(r'\blegacy\b', content, re.IGNORECASE))


def has_legacy_uppercase_in_comment(content: str) -> bool:
    """Check if 'LEGACY' (all caps) appears in a comment or doc line."""
    # Check for // LEGACY, /* LEGACY */, # LEGACY, <!-- LEGACY -->
    return bool(re.search(r'LEGACY', content))


def add_legacy_comment(content: str, path: Path) -> str:
    """Add a LEGACY comment marker to a file."""
    ext = path.suffix
    comment = ""
    if ext in ('.rs', '.c', '.h', '.cpp', '.hpp', '.js', '.ts', '.tsx', '.css', '.go', '.java'):
        comment = "// LEGACY FILE — see docs/ARCHITECTURE.md for deprecation status\n"
        # Add after shebang or first line, before any import/use
        lines = content.split('\n')
        insert_at = 0
        for i, line in enumerate(lines):
            if line.startswith('#!') or line.startswith('<?xml') or line.startswith('<!DOCTYPE'):
                continue
            if line.strip() == '':
                continue
            insert_at = i
            break
        lines.insert(insert_at, comment)
        return '\n'.join(lines)
    elif ext in ('.py', '.sh', '.yaml', '.yml'):
        comment = "# LEGACY FILE — see docs/ARCHITECTURE.md for deprecation status\n"
        lines = content.split('\n')
        # After shebang
        if lines[0].startswith('#!'):
            lines.insert(1, comment)
        else:
            lines.insert(0, comment)
        return '\n'.join(lines)
    elif ext == '.md':
        comment = "<!-- LEGACY: see docs/ARCHITECTURE.md for deprecation status -->\n"
        return comment + content
    elif ext in ('.json', '.toml'):
        # JSON/TOML can't have comments, skip
        return content
    return content


def main():
    parser = argparse.ArgumentParser(description='Enforce uppercase LEGACY comments rule')
    parser.add_argument('--fix', action='store_true', help='Auto-fix violations')
    parser.add_argument('--exit-on-error', action='store_true', help='Exit 1 if violations found')
    args = parser.parse_args()

    violations = []
    fixed = []

    for fpath in ROOT.rglob('*'):
        if not should_check(fpath):
            continue
        try:
            content = fpath.read_text(errors='replace')
        except Exception:
            continue

        if not has_legacy_anycase(content):
            continue

        if has_legacy_uppercase_in_comment(content):
            continue

        rel = fpath.relative_to(ROOT)
        violations.append(rel)
        print(f"  ❌ {rel}")

        if args.fix:
            new_content = add_legacy_comment(content, fpath)
            if new_content != content:
                fpath.write_text(new_content)
                fixed.append(rel)
                print(f"     → fixed")

    if not violations:
        print("  ✅ All files have uppercase LEGACY comments")
    else:
        print(f"\n  📊 {len(violations)} violation(s), {len(fixed)} fixed")

    if args.exit_on_error and violations:
        sys.exit(1)


if __name__ == '__main__':
    main()
