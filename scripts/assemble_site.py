#!/usr/bin/env python3
"""Assemble the ESP Web Tools installer site bundle.

Reads the per-env PlatformIO build outputs plus install/ static assets and
writes a self-contained directory (default `site/`) that can be served by
any static HTTP server — locally for testing or as a GitHub Pages artifact.

Single source of truth for the bundle layout: this script is invoked both
from the deploy-installer GitHub workflow and by developers wanting to
exercise the installer page locally.

Channel layout
--------------
When `--channel <name>` is given (e.g. "stable", "dev"), the script
publishes a two-channel layout suitable for the gh-pages branch:

    site/
    ├── index.html              # landing page: pick stable or dev
    └── <channel>/
        ├── index.html          # the ESP Web Tools installer page
        ├── connected.html
        ├── manifest-v1_1.json
        ├── manifest-v1_2.json
        └── firmware / bootloader / partitions / boot_app0

The workflow uploads this to `gh-pages` with keep_files=true so the
*other* channel's directory survives untouched.

Without `--channel`, the installer files are written flat to `site/`
(the historical layout — handy for `python -m http.server site/`
during local iteration).
"""

import argparse
import json
import re
import shutil
import sys
from pathlib import Path
from typing import Optional


def require_file(p: Path) -> Path:
    if not p.is_file():
        sys.exit(f"missing: {p}\nRun `pio run -e wireless-paper-v1_1` and "
                 f"`pio run -e wireless-paper-v1_2` first.")
    return p


def find_boot_app0() -> Path:
    p = (Path.home() / ".platformio" / "packages"
         / "framework-arduinoespressif32" / "tools" / "partitions"
         / "boot_app0.bin")
    if not p.is_file():
        sys.exit(f"boot_app0.bin not found at {p}\nBuild with PIO at least "
                 f"once so the framework package is installed.")
    return p


def write_installer_bundle(out: Path, repo: Path, version: str,
                           channel: Optional[str]) -> None:
    """Populate `out` with the installer page + firmware artefacts."""
    v12  = repo / ".pio" / "build" / "wireless-paper-v1_2"
    v11  = repo / ".pio" / "build" / "wireless-paper-v1_1"
    inst = repo / "install"

    out.mkdir(parents=True, exist_ok=True)

    # bootloader / partitions / boot_app0 are byte-identical across the two
    # envs (same chip + partition table). Take them once from v1.2.
    shutil.copy(require_file(v12 / "bootloader.bin"), out / "bootloader.bin")
    shutil.copy(require_file(v12 / "partitions.bin"), out / "partitions.bin")
    shutil.copy(find_boot_app0(),                     out / "boot_app0.bin")

    shutil.copy(require_file(v12 / "firmware.bin"), out / "firmware-v1_2.bin")
    shutil.copy(require_file(v11 / "firmware.bin"), out / "firmware-v1_1.bin")

    shutil.copy(require_file(inst / "connected.html"), out / "connected.html")

    # Inject channel + version into the installer page header. The template
    # carries placeholders that get filled here so the page can show which
    # channel the user is currently on (or fall back to "local" when run
    # without --channel for local dev).
    index_html = require_file(inst / "index.html").read_text()
    index_html = index_html.replace("{{CHANNEL}}", channel or "local")
    index_html = index_html.replace("{{VERSION}}", version)
    (out / "index.html").write_text(index_html)

    for name in ("manifest-v1_1.json", "manifest-v1_2.json"):
        data = json.loads(require_file(inst / name).read_text())
        data["version"] = version
        (out / name).write_text(json.dumps(data, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", default="dev-local",
                        help="Version string injected into manifest JSON "
                             "(default: dev-local).")
    parser.add_argument("--out", default="site",
                        help="Output directory (default: site).")
    parser.add_argument("--channel", default=None,
                        help="Channel name (e.g. 'stable', 'dev'). When "
                             "set, the installer files are placed under "
                             "site/<channel>/ and a landing index.html is "
                             "written at site/ that links to both channels. "
                             "Omit for a flat local-dev layout.")
    args = parser.parse_args()

    if args.channel is not None and not re.fullmatch(r"[a-z0-9_-]+", args.channel):
        sys.exit(f"invalid --channel {args.channel!r}: must match [a-z0-9_-]+")

    repo = Path(__file__).resolve().parents[1]
    out  = (repo / args.out).resolve()
    inst = repo / "install"

    if args.channel:
        out.mkdir(parents=True, exist_ok=True)
        # Root landing page — same content regardless of which channel is
        # being deployed, so safe to rewrite on every run.
        shutil.copy(require_file(inst / "landing.html"), out / "index.html")
        bundle_dir = out / args.channel
        write_installer_bundle(bundle_dir, repo, args.version, args.channel)
        print(f"Assembled {args.channel} installer bundle -> {bundle_dir}  "
              f"(version: {args.version})")
        print(f"Landing page -> {out / 'index.html'}")
        for p in sorted(bundle_dir.iterdir()):
            print(f"  {args.channel}/{p.name:30s} {p.stat().st_size:>10d} bytes")
    else:
        write_installer_bundle(out, repo, args.version, None)
        print(f"Assembled installer bundle -> {out}  (version: {args.version})")
        for p in sorted(out.iterdir()):
            print(f"  {p.name:30s} {p.stat().st_size:>10d} bytes")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
