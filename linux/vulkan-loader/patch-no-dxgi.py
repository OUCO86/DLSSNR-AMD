p="loader/loader_windows.c"; s=open(p).read()
a='''    wchar_t systemPath[MAX_PATH] = L"";
    GetSystemDirectoryW(systemPath, MAX_PATH);
    StringCchCatW(systemPath, MAX_PATH, L"\\\\dxgi.dll");
    HMODULE dxgi_module = LoadLibraryW(systemPath);
    fpCreateDXGIFactory1 =
        dxgi_module == NULL ? NULL : (PFN_CreateDXGIFactory1)(void *)GetProcAddress(dxgi_module, "CreateDXGIFactory1");
'''
b='''    // DLSS-NR-on-AMD: never touch DXGI. Under Proton dxgi.dll is DXVK's, whose
    // CreateDXGIFactory1 creates a Vulkan instance through this very loader
    // while holding its own lock (deadlock), and Wine's own dxgi drags OpenGL
    // in. The loader only uses DXGI to verify registry ICDs and to sort
    // physical devices; both are optional and the driver comes from the local
    // override here. A stub that fails keeps every existing check on its
    // "no DXGI" path.
    fpCreateDXGIFactory1 = nr_no_dxgi_factory;
'''
assert s.count(a)==1, "init block"
s=s.replace(a,b)
a='''PFN_CreateDXGIFactory1 fpCreateDXGIFactory1;'''
b='''PFN_CreateDXGIFactory1 fpCreateDXGIFactory1;
static HRESULT __stdcall nr_no_dxgi_factory(REFIID riid, void **ppFactory) {
    (void)riid;
    if (ppFactory) *ppFactory = NULL;
    return E_FAIL;
}'''
assert s.count(a)==1, "decl"
s=s.replace(a,b)
open(p,"w").write(s); print("patched dxgi out")
