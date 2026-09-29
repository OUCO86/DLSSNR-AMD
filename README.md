# DLSSNR-AMD

Runs the neural rendering (NR) model of NVIDIA DLSS 5 in games on AMD Radeon RX 9000 (RDNA4)
graphics cards.

- The network is reimplemented as Vulkan compute shaders.
- The model is not included. The installer extracts it from your own copy of `nvngx_dlssnr.dll`
  (version 310.8.0).
- The Linux build, on an RX 9070 XT, checked against NVIDIA's own `nvngx_dlssnr.dll` run on an
  RTX 5090 through NGX (the way a game calls it), with the same input frame and settings: PSNR
  45.56 dB at 1080p, 47.99 dB at 1440p and 49.06 dB at 4K (higher is closer; the input frame itself
  scores 26-29 dB), SSIM 0.996-0.997 (1 = identical). NVIDIA's DLL gives byte-identical output from
  run to run, so the difference is between the two implementations, not noise. Method, pictures and
  data: [docs/ngx-verification](docs/ngx-verification/NGX-VERIFICATION.md).
- **Tested only on an RX 9070 XT.** Other cards are not guaranteed to work.
- **The Windows version is still in development and has serious problems.** It is clearly slower than
  Linux, several games crash or do not work, and it is not ready for normal use. The current Windows
  code has not been tested in games at all; you have to test and, if needed, fix it yourself.

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

GPU time of the network per frame on an RX 9070 XT, **offline benchmark** (network only), measured
with v0.0.2.2:

| | 1080p | 1440p | 4K |
| --- | --- | --- | --- |
| Linux | 5.60 ms | 9.89 ms | 22.32 ms |
| Windows | 7.79 ms | - | 28.9 ms |

In game (Linux, RX 9070 XT):

| Game | Setting | Output | Render | Model | Without NR | With NR |
| --- | --- | --- | --- | --- | --- | --- |
| 007 First Light (v0.0.2) | FSR4 Performance, NR before upscaler | 4K | 1080p | 1080p | 126 fps | 68 fps |
| 007 First Light (v0.0.2) | FSR4 Performance, NR after upscaler | 4K | 1080p | 4K | 126 fps | 30 fps |
| Kingdom Come: Deliverance II (v0.0.2) | FSR4 Performance, NR before upscaler | 4K | 1080p | 1080p | 85 fps | 54 fps |
| Kingdom Come: Deliverance II (v0.0.2) | FSR4 Performance, NR after upscaler | 4K | 1080p | 4K | 85 fps | 27 fps |
| Dying Light: The Beast | FSR4 Performance, NR before upscaler | 4K | 1080p | 1080p | 97 fps | 56 fps |
| Dying Light: The Beast | FSR4 Performance, NR after upscaler | 4K | 1080p | 4K | 97 fps | 27 fps |
| Tomb Raider (2013) | Model resolution 50% | 4K | 4K | 1080p | 130 fps | 60 fps |
| Tomb Raider (2013) | Model resolution 100% | 4K | 4K | 4K | 130 fps | 30 fps |
| 7 Days to Die | FSR4 Performance, NR before upscaler | 4K | 1080p | 1080p | 136 fps | 68 fps |
| 7 Days to Die | FSR4 Performance, NR after upscaler | 4K | 1080p | 4K | 136 fps | 29 fps |

Rows marked (v0.0.2) were measured with version 0.0.2, the others with 0.0.1.

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

## Preprocess (optional)

`[Preprocess]` in `dlssnr-amd.ini` in the game folder changes the picture the NR network is shown (exposure,
display curve, contrast, saturation), and so how NR edits the picture. It has two uses: a personal look in any
game, and fixing games that do not hand their exposure to the upscaler (below). The `optiscaler` route writes
the file the first time the game starts; the ReShade routes also show the settings on the Add-ons page.

- **Enabled**: off by default; when off, none of it runs. Turned on, it starts from auto exposure and the
  filmic curve.
- **Exposure**:
  - `auto`: auto exposure after Unreal Engine's design, for games that compute their exposure but do not hand
    it to the upscaler.
  - `off`: the exposure stays as the game or OptiScaler set it.
  - `fixed`: `ExposureBias` alone.
- **ExposureBias** (EV, -8 to +8, decimals allowed): with `fixed` it is the whole gain; with `auto` it is added
  to what auto works out.
- **Curve, Contrast, Saturation**: change the picture NR is shown, and so how it edits the picture.
- **Hotkey** (Ctrl+F10 by default): switches the preprocess on and off for the current run, to compare on
  the same picture. It does not change the file.
- **Sound**: a short sound when it switches: two notes going up for on, going down for off. `Sound = 0`
  turns it off.

Some games hand the upscaler a picture without its exposure, and NR then works on a frame that is several
stops too dark: 007 First Light turns green and grainy with NR. Such games are fixed by the preprocess: set
`Enabled = 1` (its defaults, auto exposure and the filmic curve, are meant for this).

For a personal look in a game that gives its exposure properly, `auto` is not needed: use `Exposure = off`
(or `fixed` for a small shift) and change `Curve`, `Contrast` or `Saturation`. This departs from the original
look; it may be better or worse.

The first time the preprocess is turned on, NR rebuilds; a second or two of frames go without NR.
The file itself explains every setting.

## Why Vulkan (and not HIP)

HIP would work too: ROCm supports RDNA4 and its matrix (WMMA) instructions. Vulkan fits this job
better:

- The network runs on the game's own Vulkan device and queue (DXVK / vkd3d-proton under Proton). The
  frame never leaves that device, and no second GPU context or cross-API synchronisation is needed.
- Nothing extra to install: Vulkan comes with the graphics driver. HIP needs ROCm (Linux) or the HIP
  SDK (Windows), plus a bridge to reach it from inside a Wine/Proton process.
- RDNA4's matrix instructions are available in Vulkan through `VK_KHR_cooperative_matrix`, and one set
  of shaders serves Linux and Windows.

How the routes work in detail, and where the code is: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Install (Linux)

Download a `DLSSNR-AMD-Vulkan-*-x86_64.tar.gz` (64-bit games) or `-i686.tar.gz` (32-bit games) from
the releases, unpack it and run:

```sh
bash install.sh "/path/to/steamapps/common/<game folder>" --dll /path/to/nvngx_dlssnr_310.8.0.zip
```

The folder is the one holding the game's exe. The installer lists the routes, extracts the model
(below) and prints the Steam launch options to use. `--dll` is needed only for the first install:
the extracted model is kept and reused for every later one. Needs `bash` and `python3`. See
[linux/package/README.txt](linux/package/README.txt).

## The model

The weights are NVIDIA's and are not part of this project. They are extracted from your own copy of
`nvngx_dlssnr.dll`, which must be **version 310.8.0**.

- Input: the DLL itself, or a zip that contains exactly one `nvngx_dlssnr.dll` (it may be in a
  subfolder).
- A DLL of any other version is refused.
- All 599 extracted entries are checked against known hashes; the model file is written only if every
  one matches. It takes about 20 seconds and needs `bash` and `python3`.
- The extracted model is kept in the package's own `dlssnr-amd/dlssnr.bin`. Later installs from
  that package without `--dll` check its SHA256 and install it from there, so the extraction runs
  only once per package. With a new package, use `--dll` once more or copy that file over.

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
use.**

The latest update makes the Windows network faster in the **offline benchmark** (1080p 9.4 -> 7.79 ms,
4K 33 -> 28.9 ms), but this version has not been tested in any game. There is no guarantee that it
runs correctly in games; expect to test it yourself and possibly change the code to get it working.

Known issues:

- It is clearly slower than the Linux version (see Performance).
- Every game has to run on DXVK / vkd3d-proton, which changes the game's own performance and
  behaviour.
- Several games crash or do not work: DX11 games cannot use the OptiScaler route, overlays (Steam and
  others) conflict, Final Fantasy XIV crashes together with Dalamud, and the network pauses itself
  when video or system memory runs short.
- The picture is not the same as the Linux version's. The Windows network is built for the AMD
  Windows driver's shader compiler and rounds differently in places; on a 1080p test frame the two
  outputs are 48.6 dB PSNR apart.

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
