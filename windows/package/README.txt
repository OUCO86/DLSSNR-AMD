DLSSNR-AMD (Windows, experimental)
==================================

Runs the neural rendering (NR) model of DLSS 5 in games on AMD graphics cards. The network is
reimplemented in Vulkan; NVIDIA's runtime is neither needed nor called.

This Windows version is far from finished and is slower than the Linux version. Expect games
that do not work.

On Windows the game itself is D3D11/D3D12 and has no Vulkan device, so the installer puts DXVK
and vkd3d-proton into the game folder: the game runs on Vulkan and NR shares its device.

Requirements
------------
- RX 9000 series (RDNA4) graphics card, AMD driver 25.10 or newer
- 64-bit games
- Do not use it in online games with anti-cheat; start games with EasyAntiCheat with EAC off

Install
-------
Double-click install.bat, pick the game's exe (the one that actually runs - for Unreal Engine
games it is under Binaries\Win64, not the launcher outside), then pick a route:

  1) OptiScaler   DX12 games with a DLSS, FSR or XeSS option (DX12 only)
  2) ReShade      other DX10/11/12 or Vulkan games, including DX11 games with a DLSS option
  3) ReShade      old DX9 games
  4) Uninstall
  5) Collect logs when something goes wrong: writes a zip to the desktop to attach to an issue

You can also drag the game's exe onto install.bat. Games under Program Files ask for
administrator rights.

Use
---
OptiScaler: turn on DLSS (or FSR / XeSS) in the game's graphics settings. Insert opens the
            OptiScaler menu, the NR settings are on the DLSS Neural Rendering page. The
            upscaler defaults to XeSS.
            If the NR page keeps showing "Waiting for the upscaler to run", set
            [Spoofing] Dxgi=false in OptiScaler.ini and try again.
ReShade:    Home opens ReShade, the settings are on the Add-ons page; the same settings are
            kept in dlssnr-amd.ini in the game folder and changes apply live.

The first time in game the network has to compile and takes about half a minute to start;
after that it is cached.

Files put into the game folder
------------------------------
    dlssnr-amd\                  model and shaders
    d3d12.dll d3d12core.dll      vkd3d-proton
    vulkan-1.dll                 Vulkan loader (never calls DXGI; in the ReShade route it loads
                                 ReShade from the game folder)
    d3d11.dll d3d10core.dll dxgi.dll d3d9.dll   DXVK (in the OptiScaler route dxgi is
                                 OptiScaler, DXVK's dxgi is renamed dxgi-dxvk.dll, and
                                 dxgi-original.dll hands the graphics driver's own calls to
                                 the system DXGI)
    dlssnr-amd-install.txt       the installation record, which uninstall deletes by
plus the route's own files. Files of the same name already in the game folder are first moved
to dlssnr-amd-backup\ and put back on uninstall.

Logs: dlssnr-amd.log, and OptiScaler.log or ReShade.log, all in the game folder.

Known limits
------------
- The whole game runs on DXVK / vkd3d-proton; the frame rate can differ from native D3D.
- In the ReShade route, overlays such as Steam's may not show; in the OptiScaler route
  overlays are turned off (otherwise the game hangs).
- DX11 games cannot use the OptiScaler route: for DX11 games OptiScaler hands the picture to
  the system's D3D12, and on Windows DXVK's picture cannot be shared that way (7 Days to Die
  fails once a map loads). Use the ReShade route.
- Final Fantasy XIV: crashes together with Dalamud (plugin frameworks); turn Dalamud off first.
- When video memory or system memory (including virtual memory) runs short, the network pauses
  by itself and the picture returns to normal until memory frees up. Set the page file to
  "System managed size".
- 64-bit package only.

Third-party components
----------------------
DXVK (zlib licence) and vkd3d-proton (LGPL-2.1) come from GE-Proton 11-7; the versions are in
version.txt in their folders. Sources: https://github.com/doitsujin/dxvk ,
https://github.com/HansKristian-Work/vkd3d-proton . The licence files of ReShade 6.8.0,
OptiScaler-NR 0.8.4, the Vulkan Loader (with a description of the changes), vort_Shaders and
DLSS5-Feeder are in their folders.
