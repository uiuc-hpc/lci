#!/usr/bin/env python3
# NOTICE: This file has been modified from the original source.
# Changes:
# - Preserve AMD Artifactory catalog listing through the upstream merge. Date: September 8, 2026
"""List skills and agents from the AMD SLAI Marketplace on Artifactory."""


import argparse
import json
import os
import ssl
import sys
import urllib.error
import urllib.request

MANIFEST_URL = (
    "https://atlartifactory.amd.com:8443/artifactory"
    "/SW-SLAI-PROD-LOCAL/assets/manifest.json"
)


class ListError(Exception):
    pass


class Args(argparse.Namespace):
    format: str


def _codex_home() -> str:
    return os.environ.get("CODEX_HOME", os.path.join(os.getcwd(), ".codex"))


def _installed_skills() -> set[str]:
    root = os.path.join(_codex_home(), "skills")
    if not os.path.isdir(root):
        return set()
    entries = set()
    for name in os.listdir(root):
        path = os.path.join(root, name)
        if os.path.isdir(path):
            entries.add(name)
    return entries


def _fetch_manifest() -> dict:
    req = urllib.request.Request(MANIFEST_URL, headers={"User-Agent": "codex-skill-list"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        raise ListError(f"Failed to fetch marketplace manifest: HTTP {exc.code}") from exc
    except urllib.error.URLError as exc:
        raise ListError(f"Failed to connect to Artifactory: {exc.reason}") from exc


def _list_assets(manifest: dict) -> list[tuple[str, str, str]]:
    """Return sorted list of (name, type, description) tuples."""
    assets = []
    for name, entry in manifest.get("skills", {}).items():
        desc = entry.get("description", "")
        assets.append((name, "skill", desc))
    for name, entry in manifest.get("agents", {}).items():
        desc = entry.get("description", "")
        assets.append((name, "agent", desc))
    assets.sort(key=lambda a: (a[1], a[0]))
    return assets


def _parse_args(argv: list[str]) -> Args:
    parser = argparse.ArgumentParser(description="List AMD SLAI Marketplace assets.")
    parser.add_argument(
        "--format",
        choices=["text", "json"],
        default="text",
        help="Output format",
    )
    return parser.parse_args(argv, namespace=Args())


def main(argv: list[str]) -> int:
    args = _parse_args(argv)
    try:
        manifest = _fetch_manifest()
        assets = _list_assets(manifest)
        installed = _installed_skills()
        if args.format == "json":
            payload = [
                {"name": name, "type": asset_type, "installed": name in installed}
                for name, asset_type, _ in assets
            ]
            print(json.dumps(payload))
        else:
            for idx, (name, asset_type, _) in enumerate(assets, start=1):
                suffix = " (already installed)" if name in installed else ""
                print(f"{idx}. {name} [{asset_type}]{suffix}")
        return 0
    except ListError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
