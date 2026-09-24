/* winevulkan.dll forwarder: DXVK loads "winevulkan.dll" first and never looks at
   vulkan-1.dll, while ReShade's Vulkan layer looks the loader up by the name
   vulkan-1.dll. This module has no code; its one export forwards to the loader. */
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) { (void)h; (void)r; (void)p; return TRUE; }
