#!/usr/bin/env bash
# DLSSNR-AMD-Vulkan installer
#
#   bash install.sh <folder with the game's exe> [route] [--dll <nvngx_dlssnr.dll or .zip>]
#
# Routes:
#   optiscaler  the game has a DLSS option: OptiScaler, with this project as its DLSS-NR backend (64-bit only)
#   reshade     D3D10/11/12 games without a usable upscaler: ReShade + VORT motion vectors
#   vulkan      Vulkan games, or D3D9 games (DXVK turns D3D9 into Vulkan under Proton)
#   dx9         as vulkan, plus the depth direction for old D3D9 games
#   remove      uninstall
# Without a route a menu is shown.
#
# If the package has no model file, point --dll at NVIDIA's nvngx_dlssnr.dll (310.8.0, or a
# zip containing it); the model is extracted from it during installation.
#
# Every installed file is listed in dlssnr-amd-install.txt in the game folder; remove deletes by it.
set -euo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
model_name=dlssnr.bin

game="" route="" dll=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dll) dll=${2:?--dll needs a file path}; shift 2;;
        -h|--help) sed -n '2,17p' "$0"; exit 0;;
        *) if [[ -z "$game" ]]; then game=$1; elif [[ -z "$route" ]]; then route=$1;
           else echo "unexpected argument: $1" >&2; exit 1; fi; shift;;
    esac
done
[[ -n "$game" ]] || { sed -n '2,17p' "$0"; exit 1; }
[[ -d "$game" ]] || { echo "no such folder: $game" >&2; exit 1; }
game=$(cd -- "$game" && pwd)
manifest="$game/dlssnr-amd-install.txt"
bits=64; [[ -f "$here/reshade/dlssnr_amd.addon32" ]] && bits=32

exe_bits=$(python3 - "$game" <<'PY' 2>/dev/null || true
import sys, pathlib, struct
seen = set()
for exe in pathlib.Path(sys.argv[1]).glob("*.exe"):
    try:
        b = exe.read_bytes()
        pe = struct.unpack_from("<I", b, 0x3C)[0]
        seen.add({0x14C: "32", 0x8664: "64"}.get(struct.unpack_from("<H", b, pe + 4)[0], "?"))
    except Exception:
        pass
print(" ".join(sorted(seen)))
PY
)

if [[ -z "$route" ]]; then
    echo
    echo "This is the $bits-bit package; the game folder's exe is ${exe_bits:-unknown}-bit. Choose a route:"
    echo "  1) optiscaler  the game has a DLSS option (OptiScaler, 64-bit only)"
    echo "  2) reshade     D3D10/11/12 game without a usable upscaler"
    echo "  3) vulkan      Vulkan game, or D3D9 game"
    echo "  4) dx9         as 3, plus the depth setting for old D3D9 games"
    echo "  5) remove      uninstall"
    read -r -p "1-5: " pick
    case "$pick" in
        1) route=optiscaler;; 2) route=reshade;; 3) route=vulkan;; 4) route=dx9;; 5) route=remove;;
        *) echo "invalid choice" >&2; exit 1;;
    esac
fi

record() { printf '%s\n' "$1" >> "$manifest"; }
put_file() { cp -- "$1" "$game/$2"; record "$2"; }
put_tree() { mkdir -p -- "$game/$2"; cp -r -- "$1"/. "$game/$2/"; record "$2/"; }

remove_installed() {
    [[ -f "$manifest" ]] || { echo "$manifest not found, nothing to uninstall."; return; }
    while IFS= read -r entry; do
        [[ -n "$entry" ]] || continue
        case "$entry" in /*|..*|*/..*) echo "skipping suspicious entry: $entry" >&2; continue;; esac
        rm -rf -- "$game/$entry"
    done < "$manifest"
    rm -f -- "$manifest"
    echo "Uninstalled from $game."
}

case "$route" in
    remove) remove_installed; exit 0;;
    optiscaler|reshade|vulkan|dx9) ;;
    *) echo "unknown route: $route" >&2; exit 1;;
esac
if [[ "$route" == optiscaler && ! -d "$here/optiscaler" ]]; then
    echo "The OptiScaler route is only in the 64-bit package." >&2; exit 1
fi
if [[ ! -f "$here/dlssnr-amd/$model_name" && -z "$dll" ]]; then
    echo "The package has no model file: point --dll at nvngx_dlssnr.dll (310.8.0) or its zip." >&2; exit 1
fi
model_tmp=""
if [[ ! -f "$here/dlssnr-amd/$model_name" ]]; then
    model_tmp=$(mktemp)
    trap 'rm -f -- "$model_tmp"' EXIT
    echo "Extracting the model from $dll ..."
    bash "$here/model-tools/extract_model.sh" "$dll" "$model_tmp"
fi
if [[ -f "$manifest" ]]; then
    echo "Found a previous installation, removing it first."
    remove_installed
fi
if [[ -n "$exe_bits" && "$exe_bits" != "?" && "$exe_bits" != *"$bits"* ]]; then
    echo "Note: this is the $bits-bit package but the game exe is $exe_bits-bit; DLLs of the other bitness will not load." >&2
fi

: > "$manifest"; record "dlssnr-amd-install.txt"
put_tree "$here/dlssnr-amd" dlssnr-amd
[[ -n "$model_tmp" ]] && cp -- "$model_tmp" "$game/dlssnr-amd/$model_name"

case "$route" in
    optiscaler)
        tmp=$(mktemp -d)
        python3 "$here/optiscaler/extract_release.py" "$here"/optiscaler/OptiScaler*.zip "$tmp" > /dev/null
        rm -f -- "$tmp/!! EXTRACT ALL FILES TO GAME FOLDER !!" "$tmp/setup_windows.bat" "$tmp/setup_linux.sh" \
                 "$tmp/nvngx.dll_dlssnr.dll"
        mv -- "$tmp/OptiScaler.dll" "$tmp/dxgi.dll"
        for f in "$tmp"/*; do
            name=$(basename -- "$f")
            if [[ -d "$f" ]]; then put_tree "$f" "$name"; else put_file "$f" "$name"; fi
        done
        rm -rf -- "$tmp"
        for f in nvngx.dll_dlssnr.dll nvngx_dlssnr.dll _nvngx.dll; do put_file "$here/optiscaler/$f" "$f"; done
        python3 "$here/optiscaler/patch_ini.py" "$game"
        record OptiScaler.log; record dlssnr-amd.log
        overrides="dxgi=n,b"
        ;;
    reshade)
        for f in "$here"/reshade/*; do
            name=$(basename -- "$f")
            if [[ -d "$f" ]]; then put_tree "$f" "$name"; else put_file "$f" "$name"; fi
        done
        overrides="dxgi=n,b"
        ;;
    vulkan|dx9)
        for f in "$here"/reshade/* "$here"/vulkan/*; do
            name=$(basename -- "$f")
            [[ "$name" == dxgi.dll || "$name" == ReShadePreset-d3d9.ini ]] && continue
            if [[ -d "$f" ]]; then put_tree "$f" "$name"; else put_file "$f" "$name"; fi
        done
        [[ "$route" == dx9 ]] && cp -- "$here/vulkan/ReShadePreset-d3d9.ini" "$game/ReShadePreset.ini"
        overrides="winevulkan=n,b;vulkan-1=n,b"
        ;;
esac
[[ "$route" != optiscaler ]] && { record dlssnr-amd.ini; record dlssnr-amd.log; record ReShade.log; }

echo
echo "Installed the $route route into: $game"
echo "Steam launch options:  WINEDLLOVERRIDES=\"$overrides\" %command%"
if [[ "$route" == optiscaler ]]; then
    echo "In the game, turn on DLSS in its settings; Insert opens the OptiScaler menu, DLSS Neural Rendering has its own page."
    echo "If the NR page keeps showing 'Waiting for the upscaler to run', set [Spoofing] Dxgi=false in OptiScaler.ini and try again."
else
    echo "In the game, Home opens ReShade; the settings are on the Add-ons page, or edit dlssnr-amd.ini directly."
fi
echo "Logs: dlssnr-amd.log$([[ "$route" == optiscaler ]] && echo ", OptiScaler.log" || echo ", ReShade.log")"
echo "Uninstall:  bash install.sh \"$game\" remove"
