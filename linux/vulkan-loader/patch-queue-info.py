p="loader/loader.c"; s=open(p).read()
a='''VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL loader_gpa_device_terminator(VkDevice device, const char *pName) {
    struct loader_device *dev;
    struct loader_icd_term *icd_term = loader_get_icd_and_device(device, &dev);
'''
b='''// DLSS-NR-on-AMD: Wine 11's win32u finds the queue for vkGetDeviceQueue2 with a
// memcmp over the whole VkDeviceQueueInfo2, padding included (dlls/win32u/
// vulkan.c, device_find_queue). ReShade leaves the padding after sType
// uninitialised, so on a 64-bit process the lookup fails on stack leftovers,
// the queue comes back NULL and ReShade crashes in vkCreateDevice. The driver
// gets a copy whose padding is zero.
static VKAPI_ATTR void VKAPI_CALL nr_terminator_GetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo,
                                                                VkQueue *pQueue) {
    struct loader_device *dev;
    struct loader_icd_term *icd_term = loader_get_icd_and_device(device, &dev);
    PFN_vkGetDeviceQueue2 next =
        icd_term == NULL ? NULL : (PFN_vkGetDeviceQueue2)icd_term->dispatch.GetDeviceProcAddr(device, "vkGetDeviceQueue2");
    if (next == NULL) {
        if (pQueue != NULL) *pQueue = VK_NULL_HANDLE;
        return;
    }
    if (pQueueInfo == NULL) {
        next(device, pQueueInfo, pQueue);
        return;
    }
    VkDeviceQueueInfo2 info;
    memcpy(&info, pQueueInfo, sizeof(info));
    const size_t after_stype = offsetof(VkDeviceQueueInfo2, sType) + sizeof(info.sType);
    const size_t after_index = offsetof(VkDeviceQueueInfo2, queueIndex) + sizeof(info.queueIndex);
    memset((char *)&info + after_stype, 0, offsetof(VkDeviceQueueInfo2, pNext) - after_stype);
    memset((char *)&info + after_index, 0, sizeof(info) - after_index);
    next(device, &info, pQueue);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL loader_gpa_device_terminator(VkDevice device, const char *pName) {
    struct loader_device *dev;
    struct loader_icd_term *icd_term = loader_get_icd_and_device(device, &dev);
'''
assert s.count(a)==1, "terminator head"
s=s.replace(a,b)
a='''    if (icd_term == NULL) {
        return NULL;
    }

    return icd_term->dispatch.GetDeviceProcAddr(device, pName);
}
'''
b='''    if (icd_term == NULL) {
        return NULL;
    }

    if (!strcmp(pName, "vkGetDeviceQueue2")) {
        if (NULL == icd_term->dispatch.GetDeviceProcAddr(device, pName)) return NULL;
        return (PFN_vkVoidFunction)nr_terminator_GetDeviceQueue2;
    }
    return icd_term->dispatch.GetDeviceProcAddr(device, pName);
}
'''
assert s.count(a)==1, "terminator tail"
s=s.replace(a,b)
open(p,"w").write(s); print("patched vkGetDeviceQueue2 padding")
