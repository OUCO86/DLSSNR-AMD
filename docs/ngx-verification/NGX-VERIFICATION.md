# Checking DLSSNR-AMD against NVIDIA's own DLL

This compares the picture DLSSNR-AMD produces with the picture NVIDIA's DLSS 5 Neural Rendering DLL
produces for the same input and the same settings.

## What ran on each side

**NVIDIA side (the reference).** The unmodified `nvngx_dlssnr.dll` 310.8.0.0 (DLSS 5 Neural Rendering),
called through NVIDIA NGX the way a game calls it: `NVSDK_NGX_D3D12_Init_Ext`, then
`NVSDK_NGX_D3D12_CreateFeature` for the Neural Rendering feature with the `DLSSNR.*` creation parameters,
then `NVSDK_NGX_D3D12_EvaluateFeature` with the `DLSSNR.*` evaluation parameters. The DLL ran its own
host code and its own GPU kernels end to end; nothing was extracted from it, re-implemented or replayed.

- GPU: NVIDIA GeForce RTX 5090, Linux driver 580.105.08.
- NGX core: `nvngx.dll` / `_nvngx.dll` from the NVIDIA Linux driver package 615.71.09 (the Wine builds NVIDIA
  ships with the driver). The NGX cores of 580.105.08 and 595.71.05 reject this feature as out of date
  (`0xBAD0000C`), so the 615.71.09 core was used and the driver version was reported as 615.71.
- D3D12 on Linux: Wine 11.18 (staging, wow64 build), vkd3d-proton, DXVK's DXGI and dxvk-nvapi
  (DLLs from GE-Proton 11-7), on NVIDIA's Vulkan driver.
- Host program: a small D3D12 program that loads `_nvngx.dll`, creates the feature, uploads the input and
  reads the output back. `DLSSNR.Reset` is set on every evaluate, so each output is a single frame with no
  history.

**DLSSNR-AMD side.** The network shipped in DLSSNR-AMD 0.0.2.2 (unchanged in 0.0.2.3 and 0.0.2.4), run by
the Linux build on an AMD Radeon RX 9070 XT (Mesa 26.2.3 RADV), same input file, same parameters, single
frame.

## Input and settings

- Input: one Tomb Raider (2013) frame, 3840x2160, 8-bit, and its Lanczos downscales to 2560x1440 and
  1920x1080 (`inputs/`). Handed to both sides as the colour input with exact 8-bit values, constant depth,
  zero motion.
- Settings: DLSSNR-AMD's defaults on both sides: `Intensity` 1, `Style` 0, `LocalToneStrength` 1,
  `LocalStructureStrength` 1, `SkinStructureStrength` -1, `UseAutoMask` 1.

## Results

Both outputs compared as 8-bit RGB. SSIM is the mean over R, G and B (11x11 Gaussian window, sigma 1.5).
"Edit" is output minus input, i.e. what the network changed.

| Resolution | PSNR vs NVIDIA | SSIM vs NVIDIA | Correlation of the edits | Mean difference (1/255, R G B) | Pixels with all channels within one 8-bit step |
|---|---|---|---|---|---|
| 1920x1080 | 45.56 dB | 0.9961 | 0.9949 | +0.36 +0.44 +0.38 | 67.5% |
| 2560x1440 | 47.99 dB | 0.9968 | 0.9957 | +0.08 +0.09 +0.08 | 80.8% |
| 3840x2160 | 49.06 dB | 0.9970 | 0.9959 | +0.05 +0.05 +0.06 | 85.7% |

For scale, the input itself (Neural Rendering off) against NVIDIA's output: PSNR 26.44 / 27.72 / 28.69 dB,
SSIM 0.9619 / 0.9682 / 0.9727. Size of the edit (RMS, 1/255): NVIDIA 12.15 / 10.48 / 9.37,
DLSSNR-AMD 12.33 / 10.51 / 9.37.

NVIDIA's DLL gave byte-identical output in two separate runs of the same input, so the differences above
are not run-to-run noise on the NVIDIA side; they are what remains between the two implementations. They
are below one 8-bit step on average and are largest at 1080p, where DLSSNR-AMD is about 0.4/255 brighter
over smooth areas.

Pictures (`images/`): input, NVIDIA and DLSSNR-AMD side by side, for each resolution, full frame and a
close-up.

Raw outputs (`outputs/`): both sides at every resolution as lossless 8-bit PNG, exactly the pixels
compared above.

## Limits

- One picture, three resolutions, single frames. Temporal behaviour (history across frames) was not
  compared here.
- The input is handed straight to the network. How a game or OptiScaler prepares its frame before
  Neural Rendering is not part of this comparison.
