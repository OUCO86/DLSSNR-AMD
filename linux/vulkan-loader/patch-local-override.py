p="loader/loader.c"; s=open(p).read()
a='''    // Log a message when VK_LAYER_PATH is set but the override layer paths take priority
    if (manifest_type == LOADER_DATA_FILE_MANIFEST_EXPLICIT_LAYER &&'''
b='''#if defined(_WIN32)
    // DLSS-NR-on-AMD: a manifest override that lives next to the loader DLL,
    // for Wine/Proton where every process reads as high integrity and the
    // environment is ignored, and where the PnP registry walk finds nothing.
    if (NULL == override_env) {
        override_env = windows_local_override(inst, manifest_type);
    }
#endif
    // Log a message when VK_LAYER_PATH is set but the override layer paths take priority
    if (manifest_type == LOADER_DATA_FILE_MANIFEST_EXPLICIT_LAYER &&'''
assert s.count(a)==1; s=s.replace(a,b); open(p,"w").write(s)
p="loader/loader_windows.c"; s=open(p).read()
a='''void windows_initialization(void) {'''
b=r'''// DLSS-NR-on-AMD: <loader dir>\vk-override\{nr-icd.json, implicit_layer, explicit_layer}
char *windows_local_override(const struct loader_instance *inst, enum loader_data_files_type manifest_type) {
    char dll_location[MAX_PATH];
    HMODULE module_handle = NULL;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCSTR)&function_for_finding_the_current_module, &module_handle) == 0 ||
        GetModuleFileNameA(module_handle, dll_location, sizeof(dll_location)) == 0) {
        return NULL;
    }
    char *slash = strrchr(dll_location, '\\');
    if (slash == NULL) return NULL;
    *slash = 0;
    const char *tail = NULL;
    switch (manifest_type) {
        case LOADER_DATA_FILE_MANIFEST_DRIVER: tail = "\\vk-override\\nr-icd.json"; break;
        case LOADER_DATA_FILE_MANIFEST_IMPLICIT_LAYER: tail = "\\vk-override\\implicit_layer"; break;
        case LOADER_DATA_FILE_MANIFEST_EXPLICIT_LAYER: tail = "\\vk-override\\explicit_layer"; break;
        default: return NULL;
    }
    char path[MAX_PATH];
    if (snprintf(path, sizeof(path), "%s%s", dll_location, tail) >= (int)sizeof(path)) return NULL;
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return NULL;
    size_t len = strlen(path) + 1;
    char *out = (char *)loader_instance_heap_alloc(inst, len, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
    if (out == NULL) return NULL;
    memcpy(out, path, len);
    loader_log(inst, VULKAN_LOADER_INFO_BIT, 0, "DLSS-NR-on-AMD local override for manifest type %d: %s", (int)manifest_type, path);
    return out;
}

void windows_initialization(void) {'''
assert s.count(a)==1; s=s.replace(a,b); open(p,"w").write(s)
p="loader/loader_windows.h"; s=open(p).read()
a="void windows_initialization(void);"
assert s.count(a)==1
s=s.replace(a, a+"\nchar *windows_local_override(const struct loader_instance *inst, enum loader_data_files_type manifest_type);")
open(p,"w").write(s); print("patched")
