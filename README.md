# DLSSNR-AMD

Runs the neural rendering (NR) model of NVIDIA DLSS 5 in games on AMD Radeon RX 9000 (RDNA4)
graphics cards.

- The network is reimplemented as Vulkan compute shaders.
- The model is not included. The installer extracts it from your own copy of `nvngx_dlssnr.dll`
  (version 310.8.0).
- The output is close to NVIDIA's, but not identical.
- **Tested only on an RX 9070 XT.** Other cards are not guaranteed to work.
- **The Windows version is still in development and has serious problems.** It is clearly slower than
  Linux, several games crash or do not work, and it is not ready for normal use.

This is an independent project. It is not affiliated with, endorsed by or supported by NVIDIA or
AMD. DLSS is a trademark of NVIDIA Corporation.

## Before you use it

This is an early project (version 0.0.x) and its testing is limited:

- one graphics card, one Linux desktop, Mesa 26.2 and GE-Proton 11-7;
- a small number of games, at a few resolutions and settings;

Other cards, drivers, Proton versions, distributions and games have not been tested. Expect problems:
games that fail to start or crash, visual artifacts, settings that do not behave as described, or
performance that differs from the numbers below. Problems are fixed as they are found, and the project
will keep changing, including in ways that break earlier setups.

- The installer puts DLLs into the game folder. Keep a backup of anything you care about; `remove`
  deletes what was installed.
- **Do not use it in online games with anti-cheat.** Injected DLLs can get an account banned.
- It is provided as is, without warranty (see [LICENSE](LICENSE)).

If something goes wrong, please open an issue with the game, the route, your card and driver, and
`dlssnr-amd.log` from the game folder.

## Status

| Platform | Needs | State |
| --- | --- | --- |
| **Linux** (Steam / Proton) | Mesa 26.2 or newer, GE-Proton 11-7 (tested) | The main version, used in games. |
| **Windows** | AMD Software 25.10 or newer | **Experimental and far from finished.** Clearly slower than Linux, several games do not work, no ready-made packages - you have to build it yourself. |

Both need an RX 9000 series (RDNA4) card; older cards (RX 7000 and earlier) lack the FP8 matrix
instructions the network needs.

## Performance

GPU time of the network per frame on an RX 9070 XT, offline benchmark (network only):

| | 1080p | 1440p | 4K |
| --- | --- | --- | --- |
| Linux | 6.2 ms | 10.7 ms | 23.5 ms |
| Windows | 9.4 ms | - | 33 ms |

In game (Linux, RX 9070 XT):

| Game | Setting | Output | Render | Model | Without NR | With NR |
| --- | --- | --- | --- | --- | --- | --- |
| Tomb Raider (2013) | Model resolution 50% | 4K | 4K | 1080p | 130 fps | 60 fps |
| Tomb Raider (2013) | Model resolution 100% | 4K | 4K | 4K | 130 fps | 30 fps |
| 7 Days to Die | FSR4 Performance, NR before upscaler | 4K | 1080p | 1080p | 136 fps | 68 fps |
| 7 Days to Die | FSR4 Performance, NR after upscaler | 4K | 1080p | 4K | 136 fps | 29 fps |
| Dying Light: The Beast | FSR4 Performance, NR before upscaler | 4K | 1080p | 1080p | 97 fps | 56 fps |
| Dying Light: The Beast | FSR4 Performance, NR after upscaler | 4K | 1080p | 4K | 97 fps | 27 fps |

Running the model below the output resolution about doubles the frame rate. In the ReShade routes
that is the **Model resolution** setting. In the OptiScaler route it is mainly OptiScaler-NR's
**"Generate model before upscaler"** option, which runs the model on the game's render-resolution
frame before FSR4 upscales it; OptiScaler-NR has a **Model resolution** setting as well.

None of these measurements use frame generation; the OptiScaler route can turn it on.

## Supported games

| Game uses | Linux | Windows |
| --- | --- | --- |
| **DirectX 12** | `optiscaler` if the game has DLSS/FSR/XeSS, otherwise `reshade` | OptiScaler or ReShade |
| **DirectX 11** | `optiscaler` if the game has DLSS/FSR/XeSS, otherwise `reshade` | ReShade only |
| **DirectX 10** | `reshade` | ReShade |
| **DirectX 9** | `dx9` | ReShade (DX9) |
| **Vulkan** | `vulkan` | ReShade |
| OpenGL | not supported | not supported |

- **`optiscaler`**: for games with a DLSS, FSR or XeSS option. Uses
  [OptiScaler-NR](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass) with this project as its
  DLSS-NR backend, and the game's own motion vectors. 64-bit games only.
- **`reshade`**: for other DirectX 10/11/12 games. Runs through ReShade; motion is estimated from the
  picture by a ReShade shader.
- **`vulkan` / `dx9`**: for Vulkan and DirectX 9 games. On Linux, DirectX 9 games run on Vulkan through
  DXVK, and ReShade is loaded as a Vulkan layer underneath it. `dx9` also sets the depth direction old
  DirectX 9 games use.

32-bit games work on Linux (use the i686 package; not with `optiscaler`). The Windows version is
64-bit only.

## Why Vulkan and not HIP

- Game mods run inside the game's process, which on Linux is a Wine/Proton process. HIP/ROCm is not
  available there; Vulkan is, because the game already draws through it (DXVK, vkd3d-proton).
- The network runs on the game's own GPU device, so the frame is never copied to another API and back.
- The same code runs on Linux and on Windows.

How the routes work in detail, and where the code is: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Install (Linux)

Download a `DLSSNR-AMD-Vulkan-*-x86_64.tar.gz` (64-bit games) or `-i686.tar.gz` (32-bit games) from
the releases, unpack it and run:

```sh
bash install.sh "/path/to/steamapps/common/<game folder>" --dll /path/to/nvngx_dlssnr_310.8.0.zip
```

The folder is the one holding the game's exe. The installer lists the routes, extracts the model
(below) and prints the Steam launch options to use. Needs `bash` and `python3`. See
[linux/package/README.txt](linux/package/README.txt).

## The model

The weights are NVIDIA's and are not part of this project. They are extracted from your own copy of
`nvngx_dlssnr.dll`, which must be **version 310.8.0**.

- Input: the DLL itself, or a zip that contains exactly one `nvngx_dlssnr.dll` (it may be in a
  subfolder).
- A DLL of any other version is refused.
- All 599 extracted entries are checked against known hashes; the model file is written only if every
  one matches. It takes about 20 seconds and needs `bash` and `python3`.

Two ways to run it:

```sh
# while installing: the model goes to <game folder>/dlssnr-amd/dlssnr.bin
bash install.sh "<game folder>" --dll nvngx_dlssnr_310.8.0.zip

# on its own: model-tools/ in the package, linux/package/model-tools/ in this repository
bash model-tools/extract_model.sh nvngx_dlssnr_310.8.0.zip dlssnr.bin
```

On success it prints `599 entries, 140.9 MiB` (147,756,560 bytes). Put the file at
`<game folder>/dlssnr-amd/dlssnr.bin`, or pass it to a package build with `NR_MODEL=` (the Windows
package needs this; its installer does not extract the model).

## Windows

**The Windows version is still in development and has serious problems. It is not ready for normal
use.** Known issues:

- It is clearly slower than the Linux version (see Performance).
- Every game has to run on DXVK / vkd3d-proton, which changes the game's own performance and
  behaviour.
- Several games crash or do not work: DX11 games cannot use the OptiScaler route, overlays (Steam and
  others) conflict, Final Fantasy XIV crashes together with Dalamud, and the network pauses itself
  when video or system memory runs short.

There are no ready-made Windows packages; build one yourself (next section). Read
[windows/package/README.txt](windows/package/README.txt) for its known limits before trying it.

## Build

Tested on Ubuntu 24.04. Everything, including the Windows DLLs, is cross-compiled on Linux.

```sh
sudo apt install git curl python3 cmake ninja-build mingw-w64
bash fetch_deps.sh                          # pinned third-party pieces -> toolchain/, artifacts/ref/
bash linux/build/build_vulkan_loader.sh     # the patched Vulkan loader for the vulkan/dx9 routes
NR_ARCH=x86_64 bash linux/build/build_package.sh
NR_ARCH=i686   bash linux/build/build_package.sh
```

The packages land in `linux/package/`. They contain no model; `install.sh --dll` extracts it. To put a
model you extracted yourself into a package for your own use, set `NR_MODEL=/path/to/dlssnr.bin`.
Do not share packages that contain the model.

### Windows package

The Windows installer does not extract the model, so the package has to carry one:

```sh
bash fetch_deps.sh --windows                # adds GE-Proton 11-7 (DXVK, vkd3d-proton)
bash linux/package/model-tools/extract_model.sh nvngx_dlssnr_310.8.0.zip dlssnr.bin
NR_MODEL=$PWD/dlssnr.bin bash windows/build/build_package.sh
```

The result is `windows/package/DLSSNR-AMD-Windows-*-x64.zip`; on Windows, run `install.bat` from it.

## Licence

MIT for the code in this repository, see [LICENSE](LICENSE). The packages also carry third-party
components under their own licences, listed in [THIRD_PARTY.md](THIRD_PARTY.md). NVIDIA's model
weights are NVIDIA's property and are neither included nor distributed.
