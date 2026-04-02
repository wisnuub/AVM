#!/usr/bin/env python3
"""
avm-image-pull  —  Download & register Android system images for AVM.

Usage:
  avm-image-pull --android 14 --arch arm64 --flavor gapps
  avm-image-pull --android 13 --arch arm64 --flavor vanilla
  avm-image-pull --list
  avm-image-pull --check

Flavors:
  vanilla   AOSP, no Google apps
  gapps     AOSP + OpenGApps (arm64, pico package) — enables Play Store

Images land in  ~/.config/avm/images/  (Linux/macOS)
               %APPDATA%\AVM\images\   (Windows)
"""

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------
# Manifest: known compatible image sources
# arm64 generic AOSP system images from AOSP CI  (ci.android.com)
# These are the aosp_arm64-eng targets.
# For real distribution you'd host your own mirror; these point to the
# publicly available AOSP build artifacts as a reference.
# ---------------------------------------------------------------------------
MANIFEST: dict = {
    "14": {
        "arm64": {
            "vanilla": {
                "url": "https://dl.google.com/android/repository/sys-img/google_apis/arm64-v8a-34_r01.zip",
                "sha256": "placeholder_verify_before_production",
                "image_file": "system.img",
                "description": "Android 14 (API 34) ARM64 — AOSP Google APIs",
                "recommended_ram_mb": 6144,
                "recommended_cores": 4,
            },
            "gapps": {
                # GApps are injected post-download via gapps-inject.sh
                # Base image is the same Google APIs image; GApps ZIP is
                # fetched from OpenGApps (arm64/14/pico)
                "url": "https://dl.google.com/android/repository/sys-img/google_apis/arm64-v8a-34_r01.zip",
                "sha256": "placeholder_verify_before_production",
                "image_file": "system.img",
                "gapps_url": "https://github.com/opengapps/arm64/releases/download/20240901/open_gapps-arm64-14.0-pico-20240901.zip",
                "gapps_sha256": "placeholder_verify_before_production",
                "description": "Android 14 (API 34) ARM64 — GApps (Play Store) pico",
                "recommended_ram_mb": 6144,
                "recommended_cores": 4,
            },
        }
    },
    "13": {
        "arm64": {
            "vanilla": {
                "url": "https://dl.google.com/android/repository/sys-img/google_apis/arm64-v8a-33_r03.zip",
                "sha256": "placeholder_verify_before_production",
                "image_file": "system.img",
                "description": "Android 13 (API 33) ARM64 — AOSP Google APIs",
                "recommended_ram_mb": 4096,
                "recommended_cores": 4,
            },
            "gapps": {
                "url": "https://dl.google.com/android/repository/sys-img/google_apis/arm64-v8a-33_r03.zip",
                "sha256": "placeholder_verify_before_production",
                "image_file": "system.img",
                "gapps_url": "https://github.com/opengapps/arm64/releases/download/20231001/open_gapps-arm64-13.0-pico-20231001.zip",
                "gapps_sha256": "placeholder_verify_before_production",
                "description": "Android 13 (API 33) ARM64 — GApps (Play Store) pico",
                "recommended_ram_mb": 4096,
                "recommended_cores": 4,
            },
        }
    },
    "12": {
        "arm64": {
            "vanilla": {
                "url": "https://dl.google.com/android/repository/sys-img/google_apis/arm64-v8a-32_r01.zip",
                "sha256": "placeholder_verify_before_production",
                "image_file": "system.img",
                "description": "Android 12 (API 32) ARM64 — AOSP Google APIs",
                "recommended_ram_mb": 3072,
                "recommended_cores": 2,
            },
        }
    },
}


def images_dir() -> Path:
    sys_name = platform.system()
    if sys_name == "Windows":
        base = Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming"))
        return base / "AVM" / "images"
    else:
        return Path.home() / ".config" / "avm" / "images"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def progress_hook(block_num: int, block_size: int, total_size: int) -> None:
    downloaded = block_num * block_size
    if total_size > 0:
        pct = min(downloaded / total_size * 100, 100)
        bar_len = 40
        filled = int(bar_len * pct / 100)
        bar = "█" * filled + "░" * (bar_len - filled)
        mb = downloaded / 1_048_576
        total_mb = total_size / 1_048_576
        print(f"\r  [{bar}] {pct:5.1f}%  {mb:.1f}/{total_mb:.1f} MB", end="", flush=True)
    if downloaded >= total_size:
        print()


def download_file(url: str, dest: Path) -> None:
    print(f"  → {url}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    urllib.request.urlretrieve(url, dest, reporthook=progress_hook)


def extract_zip(zip_path: Path, dest_dir: Path) -> None:
    import zipfile
    print(f"  Extracting {zip_path.name} …")
    with zipfile.ZipFile(zip_path, "r") as z:
        z.extractall(dest_dir)


def inject_gapps(image_path: Path, gapps_zip: Path, android_ver: str) -> None:
    """
    Calls scripts/gapps-inject.sh to mount system.img, inject GApps, unmount.
    On macOS this requires running as root or with SIP disabled for the loop
    device. The script handles the platform-specific mount approach.
    """
    inject_script = Path(__file__).parent.parent / "scripts" / "gapps-inject.sh"
    if not inject_script.exists():
        print("  ⚠ gapps-inject.sh not found — skipping GApps injection.")
        print("    You can run it manually later:")
        print(f"    scripts/gapps-inject.sh {image_path} {gapps_zip}")
        return
    print("  Injecting GApps into system image (requires sudo) …")
    result = subprocess.run(
        ["sudo", "bash", str(inject_script), str(image_path), str(gapps_zip), android_ver],
        check=False,
    )
    if result.returncode != 0:
        print("  ⚠ GApps injection failed. See output above.")
        print("    You can re-run: sudo scripts/gapps-inject.sh <image> <gapps.zip> <api>")
    else:
        print("  ✓ GApps injected successfully.")


def register_image(slot_dir: Path, entry: dict, android_ver: str, arch: str, flavor: str) -> None:
    meta = {
        "android": android_ver,
        "arch": arch,
        "flavor": flavor,
        "description": entry["description"],
        "image_file": str(slot_dir / entry["image_file"]),
        "recommended_ram_mb": entry.get("recommended_ram_mb", 4096),
        "recommended_cores": entry.get("recommended_cores", 4),
        "gapps": flavor == "gapps",
    }
    meta_path = slot_dir / "avm-image.json"
    with open(meta_path, "w") as f:
        json.dump(meta, f, indent=2)
    print(f"  ✓ Registered → {meta_path}")


def cmd_list() -> None:
    print("\nAvailable images in manifest:\n")
    for ver, arches in MANIFEST.items():
        for arch, flavors in arches.items():
            for flavor, entry in flavors.items():
                print(f"  Android {ver}  [{arch}]  [{flavor}]  —  {entry['description']}")
    print()


def cmd_check() -> None:
    base = images_dir()
    if not base.exists():
        print(f"No images directory found at {base}")
        return
    found = list(base.glob("*/avm-image.json"))
    if not found:
        print(f"No registered images in {base}")
        return
    print(f"\nRegistered images in {base}:\n")
    for p in sorted(found):
        with open(p) as f:
            m = json.load(f)
        status = "✓" if Path(m["image_file"]).exists() else "✗ MISSING"
        gapps_flag = " [GApps]" if m.get("gapps") else ""
        print(f"  {status}  Android {m['android']} [{m['arch']}] [{m['flavor']}]{gapps_flag}")
        print(f"         {m['image_file']}")
    print()


def cmd_pull(android_ver: str, arch: str, flavor: str) -> None:
    arches = MANIFEST.get(android_ver)
    if not arches:
        print(f"Error: Android {android_ver} not in manifest. Run --list.")
        sys.exit(1)
    arch_map = arches.get(arch)
    if not arch_map:
        print(f"Error: arch '{arch}' not available for Android {android_ver}.")
        sys.exit(1)
    entry = arch_map.get(flavor)
    if not entry:
        print(f"Error: flavor '{flavor}' not available. Try 'vanilla' or 'gapps'.")
        sys.exit(1)

    slot_name = f"android{android_ver}-{arch}-{flavor}"
    slot_dir = images_dir() / slot_name
    slot_dir.mkdir(parents=True, exist_ok=True)

    image_dest = slot_dir / entry["image_file"]
    if image_dest.exists():
        print(f"Image already present at {image_dest}")
        print("Use --force to re-download.")
        return

    print(f"\n{'─'*60}")
    print(f" Pulling  Android {android_ver}  [{arch}]  [{flavor}]")
    print(f" Destination: {slot_dir}")
    print(f"{'─'*60}\n")

    # Download base image zip
    zip_path = slot_dir / "system_img.zip"
    download_file(entry["url"], zip_path)
    extract_zip(zip_path, slot_dir)
    zip_path.unlink(missing_ok=True)

    # If gapps flavor, also download + inject GApps
    if flavor == "gapps" and "gapps_url" in entry:
        gapps_zip = slot_dir / "gapps.zip"
        print("\n  Downloading OpenGApps (pico) …")
        download_file(entry["gapps_url"], gapps_zip)
        inject_gapps(image_dest, gapps_zip, android_ver)

    register_image(slot_dir, entry, android_ver, arch, flavor)

    print(f"\n✓ Done! Boot with:")
    print(f"  avm --android {android_ver} --image {image_dest}")
    if flavor == "gapps":
        print(f"  avm --android {android_ver} --image {image_dest} --gapps")
    print()


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="avm-image-pull",
        description="Download and register Android system images for AVM.",
    )
    parser.add_argument("--android", metavar="VER", help="Android version (12, 13, 14)")
    parser.add_argument("--arch", default="arm64", metavar="ARCH", help="CPU arch (default: arm64)")
    parser.add_argument(
        "--flavor",
        default="gapps",
        choices=["vanilla", "gapps"],
        help="Image flavor: vanilla (AOSP) or gapps (Play Store). Default: gapps",
    )
    parser.add_argument("--list", action="store_true", help="List available images in manifest")
    parser.add_argument("--check", action="store_true", help="List already downloaded images")
    parser.add_argument("--force", action="store_true", help="Re-download even if already present")

    args = parser.parse_args()

    if args.list:
        cmd_list()
    elif args.check:
        cmd_check()
    elif args.android:
        cmd_pull(args.android, args.arch, args.flavor)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
