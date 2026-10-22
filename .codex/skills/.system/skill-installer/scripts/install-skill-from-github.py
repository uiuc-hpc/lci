#!/usr/bin/env python3
# NOTICE: This file has been modified from the original source.
# Changes:
# - Preserve AMD Artifactory installation with staged, validated archive extraction. Date: September 8, 2026
"""Install a skill or agent from the AMD SLAI Marketplace (Artifactory)."""


import argparse
import io
import json
import os
import shutil
import stat
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile

MANIFEST_URL = (
    "https://atlartifactory.amd.com:8443/artifactory"
    "/SW-SLAI-PROD-LOCAL/assets/manifest.json"
)


class InstallError(Exception):
    pass


def _codex_home() -> str:
    return os.environ.get("CODEX_HOME", os.path.join(os.getcwd(), ".codex"))


def _default_dest() -> str:
    return os.path.join(_codex_home(), "skills")


def _fetch_manifest() -> dict:
    req = urllib.request.Request(MANIFEST_URL, headers={"User-Agent": "codex-skill-install"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        raise InstallError(f"Failed to fetch manifest: HTTP {exc.code}") from exc
    except urllib.error.URLError as exc:
        raise InstallError(f"Failed to connect to Artifactory: {exc.reason}") from exc


def _find_asset(manifest: dict, name: str) -> tuple[str, dict]:
    """Find an asset in skills or agents, return (asset_type, entry)."""
    skills = manifest.get("skills", {})
    if name in skills:
        return ("skill", skills[name])
    agents = manifest.get("agents", {})
    if name in agents:
        return ("agent", agents[name])
    raise InstallError(
        f"Asset '{name}' not found in marketplace. "
        "Run list-skills.py to see available assets."
    )


def _download_and_extract(base_url: str, entry: dict, name: str, dest_dir: str) -> None:
    latest = entry.get("latest")
    versions = entry.get("versions", {})
    if not latest or latest not in versions:
        raise InstallError(f"No downloadable version for '{name}'")
    zip_path = versions[latest]
    url = f"{base_url}/{zip_path}"

    req = urllib.request.Request(url, headers={"User-Agent": "codex-skill-install"})
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = resp.read()
    except urllib.error.HTTPError as exc:
        raise InstallError(f"Download failed for '{name}': HTTP {exc.code}") from exc
    except urllib.error.URLError as exc:
        raise InstallError(f"Download failed for '{name}': {exc.reason}") from exc

    if not data[:4].startswith(b"PK"):
        raise InstallError(f"Downloaded payload for '{name}' is not a ZIP archive")

    # Validate the entire archive before publishing any installed content.
    prefix = f"{name}/"
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        planned = []
        names = set()
        for info in zf.infolist():
            mode = (info.external_attr >> 16) & 0o170000
            expected = stat.S_IFDIR if info.is_dir() else stat.S_IFREG
            if mode not in (0, expected):
                raise InstallError(f"Special files are not allowed: {info.filename}")
            fname = info.filename
            if "\\" in fname or (len(fname) > 1 and fname[1] == ":"):
                raise InstallError(f"Non-portable archive path: {fname}")
            if fname.startswith("./"):
                fname = fname[2:]
            if fname.startswith(prefix):
                fname = fname[len(prefix):]
            if not fname or info.is_dir():
                continue
            norm = os.path.normpath(fname)
            if norm == "." or norm.startswith("..") or os.path.isabs(norm):
                raise InstallError(f"Unsafe path in archive: {info.filename}")
            if norm in names:
                raise InstallError(f"Duplicate archive path: {norm}")
            names.add(norm)
            planned.append((info, norm))

        parent = os.path.dirname(os.path.abspath(dest_dir))
        os.makedirs(parent, exist_ok=True)
        if os.path.lexists(dest_dir):
            raise InstallError(f"Destination already exists: {dest_dir}")
        with tempfile.TemporaryDirectory(prefix=".amd-skill-", dir=parent) as staging:
            content = os.path.join(staging, "content")
            os.mkdir(content)
            for info, relative in planned:
                out_path = os.path.join(content, relative)
                os.makedirs(os.path.dirname(out_path), exist_ok=True)
                with zf.open(info) as src, open(out_path, "xb") as dst:
                    shutil.copyfileobj(src, dst)
            if os.path.lexists(dest_dir):
                raise InstallError(f"Destination already exists: {dest_dir}")
            os.rename(content, dest_dir)


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Install a skill or agent from the AMD SLAI Marketplace."
    )
    parser.add_argument("assets", nargs="+", help="Asset name(s) to install")
    parser.add_argument("--dest", help="Destination skills directory")
    parser.add_argument(
        "--name",
        help="Destination name (only when installing a single asset)",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = _parse_args(argv)
    try:
        manifest = _fetch_manifest()
        base_url = manifest.get("base_url", "")
        dest_root = args.dest or _default_dest()

        for asset_name in args.assets:
            install_name = args.name if len(args.assets) == 1 and args.name else asset_name
            dest_dir = os.path.join(dest_root, install_name)
            if os.path.lexists(dest_dir):
                raise InstallError(f"Destination already exists: {dest_dir}")

            asset_type, entry = _find_asset(manifest, asset_name)
            print(f"Installing {asset_type} '{asset_name}'...")
            _download_and_extract(base_url, entry, asset_name, dest_dir)
            print(f"Installed {asset_name} to {dest_dir}")

        return 0
    except InstallError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
