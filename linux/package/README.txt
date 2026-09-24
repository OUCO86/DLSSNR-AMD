DLSSNR-AMD-Vulkan
=================

Runs the neural rendering (NR) model of DLSS 5 in games on AMD graphics cards, under Linux + Proton.
The network is reimplemented in Vulkan and runs on the game's own Vulkan device; NVIDIA's
runtime is neither needed nor called.

Requirements
------------
- RX 9000 series (RDNA4) graphics card
- Mesa 26.2 or newer (RADV driver)
- Proton: tested on GE-Proton 11-7
- python3 (the installer and the model extraction use it)

Install
-------
    bash install.sh "/path/to/steamapps/common/<game folder>" --dll /path/to/nvngx_dlssnr_310.8.0.zip

The folder is the one that holds the game's exe. The script lists the routes; you can also
name one after the folder:

    optiscaler  for games with a DLSS option. OptiScaler does the upscaling and frame
                generation, this project is its DLSS-NR backend. 64-bit package only.
    reshade     for D3D10/11/12 games without a usable upscaler. ReShade + VORT provide
                the motion vectors.
    vulkan      Vulkan games, or D3D9 games (under Proton, DXVK turns D3D9 into Vulkan).
    dx9         as vulkan, plus the depth direction for old D3D9 games.
    remove      uninstall.

When it is done the script prints the line for the Steam launch options, for example:

    WINEDLLOVERRIDES="dxgi=n,b" %command%

Use the i686 package for 32-bit games and the x86_64 package for 64-bit games.

The model
---------
The weights are NVIDIA's and are not distributed with this project. They are extracted from your
own copy of nvngx_dlssnr.dll, and it must be version 310.8.0:

- give --dll either the DLL itself or a zip that contains exactly one nvngx_dlssnr.dll
  (it may sit in a subfolder of the zip);
- a DLL of any other version is refused before anything is installed;
- every one of the 599 extracted entries is checked against known hashes, and the model is
  written only if all of them match. This takes about 20 seconds.

The result is dlssnr-amd/dlssnr.bin in the game folder (147,756,560 bytes).

To make the model file without installing anything:

    bash model-tools/extract_model.sh /path/to/nvngx_dlssnr_310.8.0.zip dlssnr.bin

It prints "599 entries, 140.9 MiB" when it succeeds. The file can then be copied to
<game folder>/dlssnr-amd/dlssnr.bin by hand.

After installation the game folder has
--------------------------------------
    dlssnr-amd/                  model and shaders; the run-time pipeline cache goes here too
    dlssnr-amd-install.txt       the installation record, which remove deletes by
plus the route's own files (OptiScaler's or ReShade's DLLs, ini files, shaders).

Use
---
optiscaler: turn on DLSS in the game; Insert opens the OptiScaler menu, the NR settings are on
            the DLSS Neural Rendering page.
            If the NR page keeps showing "Waiting for the upscaler to run", set
            [Spoofing] Dxgi=false in OptiScaler.ini (Dying Light: The Beast needs this).
reshade / vulkan / dx9: Home opens ReShade, the settings are on the Add-ons page; the same
            settings are kept in dlssnr-amd.ini in the game folder and changes apply live.

Logs: dlssnr-amd.log (and OptiScaler.log or ReShade.log), all in the game folder.

Known issues
------------
- 32-bit games at 4K may fail on the first launch (the 32-bit address space runs out while
  ReShade compiles its shaders for the first time); the second launch works.

Uninstall
---------
    bash install.sh "/path/to/<game folder>" remove
