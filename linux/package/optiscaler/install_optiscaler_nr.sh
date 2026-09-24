#!/usr/bin/env bash
# Lay out OptiScaler_DLSSNR with our AMD backend in a game directory.
#
#   install_optiscaler_nr.sh <game-dir> <OptiScaler-DLSSNR-*.zip> [dll-name]
#
# <game-dir>   the folder holding the game's .exe -- for Cyberpunk 2077 that is bin\x64, per the
#              release's own READ ME.
# <zip>        an official OptiScaler_DLSSNR release archive, unmodified.
# [dll-name]   what OptiScaler.dll is renamed to; dxgi.dll by default, which is what their installer
#              defaults to and what nearly every D3D12 game imports.
#
# What this does NOT do: build, patch or otherwise touch OptiScaler. The archive is extracted as
# shipped, its own setup_linux.sh performs the rename, and the only files added or replaced afterwards
# are ours. Run it on the build machine or on the machine holding the game; it compiles nothing.
set -euo pipefail

usage() { sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
[[ $# -ge 2 ]] || usage

game=$(realpath -m -- "$1")
zip=$(realpath -- "$2")
dllname=${3:-dxgi.dll}

here=$(cd -- "$(dirname -- "$0")" && pwd)
root=$(cd -- "$here/../.." && pwd)
dlls=${NR_OPTISCALER_DLLS:-$root/artifacts/optiscaler/nr}
payload=${NR_STANDALONE_PACKAGE:-$root/artifacts/package-build/nr/package}

[[ -d "$game" ]] || { echo "no such game directory: $game" >&2; exit 1; }
[[ -f "$zip" ]] || { echo "no such archive: $zip" >&2; exit 1; }
for f in nvngx.dll_dlssnr.dll nvngx_dlssnr.dll _nvngx.dll; do
    [[ -f "$dlls/$f" ]] || { echo "missing $dlls/$f -- run linux/build/build_optiscaler_nr.sh" >&2; exit 1; }
done
[[ -d "$payload/build" && -d "$payload/artifacts" ]] || {
    echo "missing the model payload at $payload -- run linux/build/build_package.sh" >&2; exit 1; }

echo "== 1. OptiScaler, as shipped =================================================="
python3 "$here/extract_release.py" "$zip" "$game"

echo "== 2. the rename, by their own installer ======================================"
# Non-interactive flags are the release's own (setup_linux.sh --help):
#   --using_nvidia=n   we are not on an NVIDIA card, so it must not skip the spoofing setup
#   --using_dlss=y     the game does use DLSS; that is where the pass gets depth and motion
if [[ -f "$game/OptiScaler.dll" ]]; then
    ( cd -- "$game" && bash ./setup_linux.sh --filename="$dllname" --overwrite=y \
                                             --using_nvidia=n --using_dlss=y )
else
    echo "   OptiScaler.dll already renamed; leaving $dllname alone"
fi

echo "== 3. our three DLLs =========================================================="
# nvngx.dll_dlssnr.dll REPLACES the archive's own -- theirs forwards into NVIDIA's snippet, ours runs
# the network on the AMD card. nvngx_dlssnr.dll is the file OptiScaler checks for before it will
# create a feature at all; on NVIDIA it is the 165 MB model from a driver package, here it is a copy
# of the forwarder and nothing ever loads it. _nvngx.dll is the NGX core stub.
for f in nvngx.dll_dlssnr.dll nvngx_dlssnr.dll _nvngx.dll; do
    cp -- "$dlls/$f" "$game/$f"
    printf '   %-24s %s bytes\n' "$f" "$(stat -c %s "$game/$f")"
done

echo "== 4. the network's weights and pipelines ====================================="
# The same payload the standalone module ships and loads from its own directory: nr::pe::Session is
# constructed with an empty root, which means "beside this module", and this module is beside
# OptiScaler in the game folder.
cp -r -- "$payload/build" "$game/"
cp -r -- "$payload/artifacts" "$game/"
echo "   build/ and artifacts/ copied (~290 MB of weights)"

echo "== 5. OptiScaler.ini =========================================================="
python3 "$here/patch_ini.py" "$game"

echo
echo "Done. Launch the game and open the OptiScaler overlay (Insert by default);"
echo "DLSS Neural Rendering is already enabled in the ini."
echo "If it declines, the overlay says why, and $game/OptiScaler.log has more."
echo
echo "Steam launch options, if the renamed DLL is not picked up (their installer says this too):"
echo "  WINEDLLOVERRIDES=$dllname=n,b %COMMAND%"
echo "To undo everything OptiScaler's own installer did: $game/remove_optiscaler.sh"
