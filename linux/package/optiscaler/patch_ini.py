#!/usr/bin/env python3
"""Rewrite the keys an AMD DLSS-NR install needs in OptiScaler's own OptiScaler.ini.

Key names and their sections were read from the shipped OptiScaler.ini and from OptiScaler/Config.cpp
(`readBool("DlssNr", "Enabled")`, and the [Libraries] block that holds NvngxPath). Nothing is added
that is not already in the file: every key here ships as `<key>=auto` and is rewritten in place, so a
key that has been renamed upstream shows up as an error rather than as a silently ignored line.
"""

import os
import re
import sys


def windows_path(p: str) -> str:
    """The path as the game will see it.

    OptiScaler reads this with std::filesystem inside Wine, so an absolute unix path has to arrive as
    a Windows one. Wine maps the whole unix root onto Z:, which is what makes this exact rather than a
    guess. A path that already carries a drive letter -- a native Windows install, or one already
    prefixed -- is left alone.
    """
    p = p.replace("/", "\\")
    if len(p) > 1 and p[1] == ":":
        return p
    return "Z:" + p if p.startswith("\\") else p


def wanted_keys(game: str) -> dict:
    return {
        # The pass itself. Ships false; this is the whole point of the install.
        ("DlssNr", "Enabled"): "true",
        # Where NVNGXProxy looks for the NGX core. Util::LoadProxyLibrary takes either a directory (it
        # appends _nvngx.dll, the first of the two names it tries) or a file path; the file path is
        # written so there is no doubt which name it picks up.
        ("Libraries", "NvngxPath"): windows_path(os.path.join(game, "_nvngx.dll")),
        # Spoofing. [Spoofing] Dxgi already defaults to true on an AMD card ("Default (auto) is true
        # for AMD/Intel") and SpoofedVendorId/SpoofedDeviceId already default to NVIDIA and a 4090.
        # Written out only so an inherited ini cannot have turned them off.
        # [Spoofing] Dxgi is left at the release's own `auto` on purpose: it is
        # per game. Dying Light: The Beast only lets the NR pass run with it OFF
        # (found 2026-09-16), other games need it ON to expose DLSS at all.
        ("Spoofing", "StreamlineSpoofing"): "true",
        # [Upscalers] Dx12Upscaler is deliberately absent: left at auto it picks FSR4 on a capable
        # Radeon and XeSS otherwise, which is OptiScaler's own choice and has nothing to do with
        # Neural Rendering.
    }


def main(game: str) -> int:
    path = os.path.join(game, "OptiScaler.ini")
    wanted = wanted_keys(game)
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()

    section = None
    seen = set()
    for i, line in enumerate(lines):
        header = re.match(r"\s*\[([^\]]+)\]", line)
        if header:
            section = header.group(1)
            continue
        # Live keys only. A commented-out line is documentation, and turning one into a setting would
        # both duplicate the real key and enable something nobody asked for.
        entry = re.match(r"\s*([A-Za-z0-9_]+)\s*=", line)
        if not entry or section is None:
            continue
        key = (section, entry.group(1))
        if key in wanted and key not in seen:
            seen.add(key)
            lines[i] = f"{entry.group(1)}={wanted[key]}"

    open(path, "w", encoding="utf-8").write("\n".join(lines) + "\n")

    for (s, k), v in sorted(wanted.items()):
        print(f"  {' ' if (s, k) in seen else '!'} [{s}] {k}={v}")

    missing = sorted(k for k in wanted if k not in seen)
    if missing:
        print(f"  ! not found in {path}; add them by hand: {missing}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <game-dir>", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(main(sys.argv[1]))
