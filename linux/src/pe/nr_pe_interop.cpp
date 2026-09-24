#include "nr_pe_interop.hpp"
#include "nr_pe_log.hpp"
#include <cstdio>
#include <mutex>
#include <set>
#include <string>

namespace nr::pe {
namespace {
// vkd3d-proton hands out VkFormat-equivalents through the D3D12 description, so
// the texture's own D3D12 format is what tells us how to treat the image. Only
// the formats a colour or motion buffer actually uses are listed; anything else
// is declined rather than guessed at, because guessing a format here silently
// reinterprets a game's memory.
//
// The value returned is used for two things only: choosing the runtime's
// transfer mode (8-bit through the UNORM detour, everything else a direct
// blit) and the runtime's own bookkeeping. The blits themselves go by the
// VkImage's real format, which vkd3d chose when it created the image, so a
// wrong entry here cannot reinterpret bits - it can only pick the wrong
// transfer mode.
//
// TYPELESS: a game that creates its colour buffer typeless views it as the
// typed member of the same family. vkd3d creates the VkImage with a
// representative typed VkFormat and MUTABLE_FORMAT_BIT for the views, so the
// blit reads the image the way vkd3d chose to. The family decides the transfer
// mode - 8-bit families take the UNORM detour, which is what the SRGB member
// gets too - and the entry below is that representative. This is inferred
// from vkd3d-proton's format table as remembered, not read from its source on
// this machine; a typeless resource is logged as such so a wrong picture can
// be traced back here.
VkFormat vulkan_format(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return VK_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return VK_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    // The usual pre-upscale HDR colour buffer: three unsigned floats, no alpha,
    // no transfer function. The values are linear scene-referred and can
    // exceed 1.0; the pass carries them as-is.
    case DXGI_FORMAT_R11G11B10_FLOAT: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case DXGI_FORMAT_R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case DXGI_FORMAT_R16G16_FLOAT: return VK_FORMAT_R16G16_SFLOAT;
    case DXGI_FORMAT_R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
    case DXGI_FORMAT_R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
    case DXGI_FORMAT_R32G32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
    case DXGI_FORMAT_R16G16_SNORM: return VK_FORMAT_R16G16_SNORM;
    default: return VK_FORMAT_UNDEFINED;
    }
}

bool is_typeless(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return true;
    default: return false;
    }
}
}  // namespace

VkFormat vulkan_format_of(DXGI_FORMAT format) { return vulkan_format(format); }

DeviceHandles device_handles(ID3D12Device* device) {
    DeviceHandles out{};
    if (!device) return out;
    ID3D12DXVKInteropDevice* interop = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(ID3D12DXVKInteropDevice),
                                      reinterpret_cast<void**>(&interop))) || !interop)
        return out;   // not vkd3d-proton: nothing underneath to reach
    interop->GetVulkanHandles(&out.instance, &out.physical, &out.device);
    interop->Release();
    return out;
}

VkCommandBuffer command_buffer(ID3D12GraphicsCommandList* list) {
    if (!list) return VK_NULL_HANDLE;
    ID3D12GraphicsCommandListExt* ext = nullptr;
    if (FAILED(list->QueryInterface(__uuidof(ID3D12GraphicsCommandListExt),
                                    reinterpret_cast<void**>(&ext))) || !ext)
        return VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    ext->GetVulkanHandle(&cmd);
    ext->Release();
    return cmd;
}

ResourceHandle resource_handle(ID3D12Device* device, ID3D12Resource* resource,
                               D3D12_RESOURCE_STATES state) {
    ResourceHandle out{};
    if (!device || !resource) return out;
    ID3D12DXVKInteropDevice* interop = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(ID3D12DXVKInteropDevice),
                                      reinterpret_cast<void**>(&interop))) || !interop)
        return out;
    UINT64 handle = 0, offset = 0;
    if (SUCCEEDED(interop->GetVulkanResourceInfo(resource, &handle, &offset)) && handle) {
        // A C-style cast: VkImage is a pointer on x86_64 and a uint64_t on i686,
        // and only one of static_cast/reinterpret_cast is legal for each.
        out.image = (VkImage)handle;
        interop->GetVulkanImageLayout(resource, state, &out.layout);
        const auto desc = resource->GetDesc();
        out.width = uint32_t(desc.Width);
        out.height = uint32_t(desc.Height);
        out.dxgi = desc.Format;
        out.format = vulkan_format(desc.Format);
        // Kept even when the format is unknown: the handle carries what the
        // format *was*, and that is the one thing a rejection has to report.
    }
    interop->Release();
    return out;
}

DeviceHandles d3d11_handles(IUnknown* device, IDXGIVkInteropDevice** interop) {
    DeviceHandles out{};
    if (interop) *interop = nullptr;
    if (!device) return out;
    IDXGIVkInteropDevice* dxvk = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(IDXGIVkInteropDevice),
                                      reinterpret_cast<void**>(&dxvk))) || !dxvk)
        return out;   // not DXVK: nothing underneath to reach
    dxvk->GetVulkanHandles(&out.instance, &out.physical, &out.device);
    if (interop) *interop = dxvk; else dxvk->Release();
    return out;
}

// ReShade hooks every D3D11 resource's GetDevice (vtable slot 3, all
// instances of the class) so that it returns ReShade's proxy device, found
// through a private-data pointer it stores on the real device under its own
// class GUID. DXVK's IDXGIVkInteropSurface::GetVulkanImageInfo calls that
// GetDevice on first use and static_casts the result to its own device class
// to pin the image; on the proxy that is garbage, and it dies acquiring an
// SRW lock inside ntdll - read out of both sources and matched to the
// faulting instruction. So for the duration of the interop call the private
// pointer is taken off the real device, and put back after. ReShade's own
// comment says the substitution exists for ANGLE and Media Foundation, which
// are not running in the middle of a ReShade technique.
namespace {
const GUID kReShadeD3D11DeviceProxy =
    {0x72299288, 0x2C68, 0x4AD8, {0x94, 0x5D, 0x2B, 0xFB, 0x5A, 0xA9, 0xC6, 0x09}};

// **Once per resource, not once per frame.**
//
// The step-by-step trace below was written while this path was killing a 32-bit
// game on its first call, and once the path worked nobody took it back out: it
// runs for the colour, motion and depth textures of every frame, so at 4K it
// wrote fifteen flushed lines a frame - 2.7 MB and 35,689 lines in one session,
// with the four lines anybody needed buried in the middle of it. A file write
// per line per frame is also not free in the frame it is written in.
//
// Each distinct texture is traced the first time it is seen and then goes
// quiet. A recycled pointer reads as already-seen, which costs a trace nobody
// was going to read; the set is cleared if a game ever churns enough resources
// to make it grow, since it exists to suppress repetition, not to be a census.
bool first_sight(const void* texture) {
    static std::mutex lock;
    static std::set<const void*> seen;
    std::lock_guard<std::mutex> guard(lock);
    if (seen.size() > 64) seen.clear();
    return seen.insert(texture).second;
}

struct ProxyHidden {
    ID3D11Device* real{};
    void* proxy{};
    bool active{};
    bool trace{};
    void hide(ID3D11Resource* resource, IUnknown* device) {
        if (!resource || !device) return;
        if (FAILED(device->QueryInterface(__uuidof(ID3D11Device), reinterpret_cast<void**>(&real))) || !real)
            return;
        ID3D11Device* seen = nullptr;
        resource->GetDevice(&seen);
        const bool proxied = seen && seen != real;
        if (seen) seen->Release();
        if (!proxied) return;
        UINT size = sizeof(proxy);
        if (SUCCEEDED(real->GetPrivateData(kReShadeD3D11DeviceProxy, &size, &proxy)) && proxy) {
            real->SetPrivateData(kReShadeD3D11DeviceProxy, 0, nullptr);
            active = true;
            if (trace)
                log("[nr] d3d11_image: GetDevice is proxied (%p, real %p); hiding the proxy for the "
                    "interop call", static_cast<void*>(seen), static_cast<void*>(real));
        } else if (trace) {
            log("[nr] d3d11_image: GetDevice is proxied by something that is not ReShade's device; "
                "the DXVK interop call may fault");
        }
    }
    ~ProxyHidden() {
        if (active) real->SetPrivateData(kReShadeD3D11DeviceProxy, sizeof(proxy), &proxy);
        if (real) real->Release();
    }
};
}  // namespace

ResourceHandle d3d11_image(IUnknown* texture, IUnknown* device) {
    ResourceHandle out{};
    if (!texture) return out;
    // Traced step by step the first time each texture is seen: this is the call
    // a 32-bit D3D11 game died in, and the path had never run in any game
    // before. See first_sight for why it is not every frame any more.
    const bool trace = first_sight(texture);
    ID3D11Resource* as_resource = nullptr;
    const HRESULT is_resource = texture->QueryInterface(__uuidof(ID3D11Resource),
                                                        reinterpret_cast<void**>(&as_resource));
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    if (SUCCEEDED(is_resource) && as_resource) { as_resource->GetType(&dimension); as_resource->Release(); }
    if (trace)
        log("[nr] d3d11_image: %p is %s (dimension %d)", static_cast<void*>(texture),
            SUCCEEDED(is_resource) ? "an ID3D11Resource" : "NOT an ID3D11Resource", int(dimension));
    ProxyHidden hidden;
    hidden.trace = trace;
    if (SUCCEEDED(is_resource) && as_resource) {
        // as_resource was released above; take it again for the duration.
        ID3D11Resource* again = nullptr;
        if (SUCCEEDED(texture->QueryInterface(__uuidof(ID3D11Resource), reinterpret_cast<void**>(&again))) && again) {
            hidden.hide(again, device);
            again->Release();
        }
    }
    IDXGIVkInteropSurface* surface = nullptr;
    const HRESULT qi = texture->QueryInterface(__uuidof(IDXGIVkInteropSurface),
                                               reinterpret_cast<void**>(&surface));
    if (trace)
        log("[nr] d3d11_image: IDXGIVkInteropSurface query 0x%08lx, surface %p", long(qi),
            static_cast<void*>(surface));
    if (FAILED(qi) || !surface) return out;
    // DXVK's contract for pInfo (dxgi_interfaces.h): sType must be
    // VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, and pQueueFamilyIndices must point
    // to a caller-owned array whose length is queueFamilyIndexCount. A
    // zero-initialised struct broke both rules and DXVK 3.1 wrote through the
    // null pointer - the access violation in ntdll's memcpy that took Tomb
    // Raider 2013 down. Handle and layout first, with no info at all, because
    // that call has the fewest ways to fail; the description second, and the
    // D3D11 description as the fallback if DXVK declines it.
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image = VK_NULL_HANDLE;
    const HRESULT got = surface->GetVulkanImageInfo(&image, &layout, nullptr);
    if (trace)
        log("[nr] d3d11_image: handle/layout 0x%08lx, image %llx, layout %u", long(got),
            (unsigned long long)(uintptr_t)image, unsigned(layout));
    if (SUCCEEDED(got) && image) {
        out.image = image;
        out.layout = layout;
        uint32_t families[16] = {};
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.queueFamilyIndexCount = 16;
        info.pQueueFamilyIndices = families;
        const HRESULT described = surface->GetVulkanImageInfo(nullptr, nullptr, &info);
        if (trace)
            log("[nr] d3d11_image: description 0x%08lx, %ux%u format %u usage 0x%x", long(described),
                info.extent.width, info.extent.height, unsigned(info.format), unsigned(info.usage));
        if (SUCCEEDED(described) && info.extent.width && info.format != VK_FORMAT_UNDEFINED) {
            out.format = info.format;
            out.width = info.extent.width;
            out.height = info.extent.height;
            out.usage = info.usage;
        } else {
            // DXVK creates every D3D11 texture with transfer usage on both
            // sides; the rest comes from the texture's own description.
            ID3D11Texture2D* tex = nullptr;
            D3D11_TEXTURE2D_DESC desc{};
            if (SUCCEEDED(texture->QueryInterface(__uuidof(ID3D11Texture2D),
                                                  reinterpret_cast<void**>(&tex))) && tex) {
                tex->GetDesc(&desc);
                tex->Release();
            }
            out.width = desc.Width; out.height = desc.Height;
            out.format = vulkan_format(desc.Format);
            out.dxgi = desc.Format;
            out.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            if (trace)
                log("[nr] d3d11_image: using the D3D11 description instead: %ux%u DXGI %u -> VkFormat %u",
                    out.width, out.height, unsigned(desc.Format), unsigned(out.format));
            if (out.format == VK_FORMAT_UNDEFINED) out = ResourceHandle{};
        }
    }
    surface->Release();
    return out;
}

bool d3d12_queue_access(ID3D12Device* device, QueueAccess* out, ID3D12CommandQueue** owned) {
    if (!device || !out || !owned) return false;
    ID3D12DXVKInteropDevice* interop = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(ID3D12DXVKInteropDevice),
                                      reinterpret_cast<void**>(&interop))) || !interop)
        return false;
    // Our own queue rather than the game's: we never have to find the game's, and
    // vkd3d maps both onto the same vkd3d_queue, so the lock below still excludes
    // the game. DIRECT because the weight upload copies as well as computes.
    ID3D12CommandQueue* queue = nullptr;
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue),
                                          reinterpret_cast<void**>(&queue))) || !queue) {
        interop->Release();
        return false;
    }
    VkQueue vk_queue = VK_NULL_HANDLE;
    uint32_t family = 0;
    if (FAILED(interop->GetVulkanQueueInfo(queue, &vk_queue, &family)) || !vk_queue) {
        queue->Release();
        interop->Release();
        return false;
    }
    out->queue = vk_queue;
    out->family = family;
    // The interop device is kept alive by the closures, which the session holds
    // for as long as it holds the queue; releasing it here would outlive nothing.
    interop->AddRef();
    out->lock = [interop, queue] { interop->LockCommandQueue(queue); };
    out->unlock = [interop, queue] { interop->UnlockCommandQueue(queue); };
    // No flush on this path in the first place - vkd3d's lock is the queue's own
    // mutex and nothing here touches an immediate context - so the quiet pair is
    // the same pair.
    out->lock_quiet = out->lock;
    out->unlock_quiet = out->unlock;
    *owned = queue;   // caller releases; that also releases our AddRef's peer
    interop->Release();
    return true;
}

std::string describe_rejection(const ResourceHandle& handle, const char* what) {
    char text[256];
    if (!handle) {
        std::snprintf(text, sizeof text,
                      "vkd3d-proton returned no Vulkan image for the %s resource", what);
    } else {
        std::snprintf(text, sizeof text,
                      "the %s resource is DXGI format %u at %ux%u, which this pass does not accept",
                      what, unsigned(handle.dxgi), handle.width, handle.height);
    }
    return text;
}

bool resource_is_typeless(const ResourceHandle& handle) { return is_typeless(handle.dxgi); }

bool d3d11_queue_access(IDXGIVkInteropDevice* interop, QueueAccess* out) {
    if (!interop || !out) return false;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t family = 0;
    interop->GetSubmissionQueue(&queue, &family);
    if (!queue) return false;
    out->queue = queue;
    out->family = family;
    // DXVK's documented sequence. The flush is part of the lock rather than a
    // separate step because every caller needs it: it puts everything the game
    // has already recorded in front of whatever we are about to submit.
    out->lock = [interop] {
        interop->FlushRenderingCommands();
        interop->LockSubmissionQueue();
    };
    out->unlock = [interop] { interop->ReleaseSubmissionQueue(); };
    // The queue mutex alone, for a thread that does not own the immediate
    // context; see QueueAccess::lock_quiet for what calling the pair there did.
    out->lock_quiet = [interop] { interop->LockSubmissionQueue(); };
    out->unlock_quiet = [interop] { interop->ReleaseSubmissionQueue(); };
    return true;
}

}  // namespace nr::pe
