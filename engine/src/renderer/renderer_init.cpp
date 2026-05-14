// One-time Vulkan setup for the renderer.
//
// Holds RendererInstance (phase 1: VkInstance + debug messenger), the
// bindSurface transition that arms a Renderer (phase 2: device, queue, sync
// primitives, command pool, swapchain), and Renderer::recreateSwapchain_.
// Recreate is a Renderer member, not strictly init-only, but it lives here
// because it shares the anon-namespace helper (createRenderCompleteSemaphores)
// with bindSurface.
//
// Per-frame Renderer ops and lifetime management (move ops, destroy_) live
// in renderer.cpp.

#include "renderer/renderer.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include "volk.h"
#include "vk_check.h"

namespace
{
    struct PhysicalDeviceInfo
    {
        VkPhysicalDevice device;
        VkPhysicalDeviceProperties properties;
        uint32_t score;
        uint32_t graphicsQueueIdx; // also supports present to the surface used for picking
    };

    std::expected<std::vector<VkLayerProperties>, VkResult> getSupportedLayers()
    {
        uint32_t count = 0;
        VK_TRY(vkEnumerateInstanceLayerProperties(&count, nullptr));
        std::vector<VkLayerProperties> layers(count);
        VK_TRY(vkEnumerateInstanceLayerProperties(&count, layers.data()));
        return layers;
    }

    std::expected<std::vector<VkExtensionProperties>, VkResult> getSupportedExtensions()
    {
        uint32_t count = 0;
        VK_TRY(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
        std::vector<VkExtensionProperties> exts(count);
        VK_TRY(vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data()));
        return exts;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                       VkDebugUtilsMessageTypeFlagsEXT types,
                                                       const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                       void* /*userData*/)
    {
        spdlog::level::level_enum level = spdlog::level::info;
        switch (severity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            level = spdlog::level::trace;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            level = spdlog::level::info;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            level = spdlog::level::warn;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            level = spdlog::level::err;
            break;
        default:
            break;
        }

        // Type is a flag mask; pick the most specific tag in priority order
        // so the log prefix tells you whether to blame your code (validation),
        // your usage pattern (performance), or the loader (general).
        const char* tag = "general";
        if (types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            tag = "validation";
        else if (types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
            tag = "performance";

        spdlog::log(level, "[vulkan/{}] {}", tag, data->pMessage);
        // VK_FALSE so the triggering API call still proceeds — VK_TRUE is for
        // fuzz harnesses that want to abort the offending call.
        return VK_FALSE;
    }

    enum class DeviceSelectError : std::uint8_t
    {
        // vkEnumeratePhysicalDevices returned a non-success VkResult. The
        // underlying code is logged at the call site; not preserved in the
        // error value because callers haven't needed to branch on it yet.
        EnumerationFailed,
        // Enumeration succeeded but reported zero devices. Distinct from
        // EnumerationFailed: no Vulkan-capable GPU/driver is installed,
        // not a transient failure.
        NoPhysicalDevices,
        // Devices exist but none meet the renderer's requirements
        // (queue family with graphics + present support for the surface,
        // required extensions, etc).
        NoSuitableDevice,
    };

    // Score a physical device against the bound surface. Score 0 means
    // unsuitable; the highest-scoring device wins. Integrated GPUs are
    // preferred over discrete to keep idle power draw low — sized so type
    // dominates and leaves room for tiebreakers (VRAM, driver) without
    // swamping the type preference. Suitability requires at least one
    // queue family that supports both graphics and present to `surface`.
    PhysicalDeviceInfo scoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        uint32_t queueFamilyCount{0};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        std::optional<uint32_t> queueIdx;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (!(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            VkBool32 presentSupport = VK_FALSE;
            // Failure here is treated as "no present support on this queue family";
            // the underlying VkResult is dropped because callers haven't needed it.
            if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport) != VK_SUCCESS) continue;
            if (presentSupport)
            {
                queueIdx = i;
                break;
            }
        }
        if (!queueIdx)
        {
            return PhysicalDeviceInfo{.device = device, .properties = properties, .score = 0, .graphicsQueueIdx = 0};
        }

        uint32_t typeBonus = 0;
        switch (properties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            typeBonus = 1000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            typeBonus = 100;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            typeBonus = 10;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            typeBonus = 1;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        default:
            typeBonus = 0;
            break;
        }

        return PhysicalDeviceInfo{
            .device = device,
            .properties = properties,
            .score = 1 + typeBonus,
            .graphicsQueueIdx = *queueIdx,
        };
    }

    std::expected<PhysicalDeviceInfo, DeviceSelectError> pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
    {
        uint32_t deviceCount{0};
        if (const VkResult r = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr); r != VK_SUCCESS)
        {
            spdlog::error("[vulkan] vkEnumeratePhysicalDevices (count) failed: {}", renderer::vkResultString(r));
            return std::unexpected(DeviceSelectError::EnumerationFailed);
        }
        if (deviceCount == 0)
        {
            spdlog::error("[vulkan] no Vulkan physical devices present — is a GPU driver installed?");
            return std::unexpected(DeviceSelectError::NoPhysicalDevices);
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        if (const VkResult r = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()); r != VK_SUCCESS)
        {
            spdlog::error("[vulkan] vkEnumeratePhysicalDevices (fill) failed: {}", renderer::vkResultString(r));
            return std::unexpected(DeviceSelectError::EnumerationFailed);
        }

        std::vector<PhysicalDeviceInfo> scored;
        scored.reserve(devices.size());
        for (VkPhysicalDevice d : devices)
            scored.push_back(scoreDevice(d, surface));

        auto best = std::ranges::max_element(scored, {}, &PhysicalDeviceInfo::score);
        if (best == scored.end() || best->score == 0)
        {
            spdlog::error("[vulkan] {} physical device(s) found but none are suitable", devices.size());
            return std::unexpected(DeviceSelectError::NoSuitableDevice);
        }

        spdlog::info("[vulkan] selected physical device: {} (score {})", best->properties.deviceName, best->score);
        return *best;
    }

    std::expected<VkDevice, VkResult> createLogicalDevice(PhysicalDeviceInfo physicalDevice)
    {
        constexpr float qfpriorities{1.0f};
        VkDeviceQueueCreateInfo queueCI{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                        .queueFamilyIndex = physicalDevice.graphicsQueueIdx,
                                        .queueCount = 1,
                                        .pQueuePriorities = &qfpriorities};
        VkPhysicalDeviceVulkan12Features enabledVk12Features{.sType =
                                                                 VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                                                             .descriptorIndexing = true,
                                                             .shaderSampledImageArrayNonUniformIndexing = true,
                                                             .descriptorBindingVariableDescriptorCount = true,
                                                             .runtimeDescriptorArray = true,
                                                             .bufferDeviceAddress = true};
        VkPhysicalDeviceVulkan13Features enabledVk13Features{.sType =
                                                                 VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                                                             .pNext = &enabledVk12Features,
                                                             .synchronization2 = true,
                                                             .dynamicRendering = true};
        VkPhysicalDeviceFeatures enabledVk10Features{.samplerAnisotropy = VK_TRUE};
        const std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo deviceCI{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                    .pNext = &enabledVk13Features,
                                    .queueCreateInfoCount = 1,
                                    .pQueueCreateInfos = &queueCI,
                                    .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                                    .ppEnabledExtensionNames = deviceExtensions.data(),
                                    .pEnabledFeatures = &enabledVk10Features};

        VkDevice device = VK_NULL_HANDLE;
        VK_TRY(vkCreateDevice(physicalDevice.device, &deviceCI, nullptr, &device));

        return device;
    }

    struct FrameSync
    {
        std::array<VkFence, renderer::MAX_FRAMES_IN_FLIGHT> fences{};
        std::array<VkSemaphore, renderer::MAX_FRAMES_IN_FLIGHT> imageAcquiredSemaphores{};
    };

    // Per-frame-in-flight CPU/GPU sync. The fence is pre-signaled so frame 0's
    // wait doesn't deadlock against work that's never been submitted; the
    // imageAcquired semaphore is what vkAcquireNextImageKHR signals before the
    // submit can proceed.
    std::expected<FrameSync, VkResult> createFrameSync(VkDevice device)
    {
        FrameSync sync;
        const VkFenceCreateInfo fci{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        const VkSemaphoreCreateInfo sci{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        // Clean up whatever we've created so far on failure. vkDestroy* on
        // VK_NULL_HANDLE is a documented no-op, so the slots we never reached
        // are harmless to sweep.
        auto cleanup = [&]
        {
            for (uint32_t i = 0; i < renderer::MAX_FRAMES_IN_FLIGHT; ++i)
            {
                if (sync.fences[i] != VK_NULL_HANDLE) vkDestroyFence(device, sync.fences[i], nullptr);
                if (sync.imageAcquiredSemaphores[i] != VK_NULL_HANDLE)
                    vkDestroySemaphore(device, sync.imageAcquiredSemaphores[i], nullptr);
            }
        };
        for (uint32_t i = 0; i < renderer::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (const VkResult r = vkCreateFence(device, &fci, nullptr, &sync.fences[i]); r != VK_SUCCESS)
            {
                spdlog::error("[vulkan] vkCreateFence[{}] failed: {}", i, renderer::vkResultString(r));
                cleanup();
                return std::unexpected(r);
            }
            if (const VkResult r = vkCreateSemaphore(device, &sci, nullptr, &sync.imageAcquiredSemaphores[i]);
                r != VK_SUCCESS)
            {
                spdlog::error("[vulkan] vkCreateSemaphore (imageAcquired)[{}] failed: {}", i,
                              renderer::vkResultString(r));
                cleanup();
                return std::unexpected(r);
            }
        }
        return sync;
    }

    struct CommandResources
    {
        VkCommandPool pool = VK_NULL_HANDLE;
        std::array<VkCommandBuffer, renderer::MAX_FRAMES_IN_FLIGHT> buffers{};
    };

    // Command pool + the per-frame primary command buffers allocated out of it.
    // Pool gets RESET_COMMAND_BUFFER_BIT so each frame can reset its own buffer
    // once its fence signals, instead of resetting the whole pool. Buffers are
    // owned by the pool: destroying the pool frees them.
    std::expected<CommandResources, VkResult> createCommandResources(VkDevice device, uint32_t queueFamilyIdx)
    {
        CommandResources out;
        const VkCommandPoolCreateInfo pci{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIdx,
        };
        if (const VkResult r = vkCreateCommandPool(device, &pci, nullptr, &out.pool); r != VK_SUCCESS)
        {
            spdlog::error("[vulkan] vkCreateCommandPool failed: {}", renderer::vkResultString(r));
            return std::unexpected(r);
        }
        const VkCommandBufferAllocateInfo ai{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = out.pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = renderer::MAX_FRAMES_IN_FLIGHT,
        };
        if (const VkResult r = vkAllocateCommandBuffers(device, &ai, out.buffers.data()); r != VK_SUCCESS)
        {
            spdlog::error("[vulkan] vkAllocateCommandBuffers failed: {}", renderer::vkResultString(r));
            vkDestroyCommandPool(device, out.pool, nullptr);
            return std::unexpected(r);
        }
        return out;
    }

    // One semaphore per swapchain image: queue-submit signals it, vkQueuePresentKHR
    // waits on it. Sized by image count, not MAX_FRAMES_IN_FLIGHT, because the
    // present engine indexes by image — two frames-in-flight can land on the
    // same image, and the "renderComplete for image N" semaphore must outlive
    // the submit that signaled it until the present-wait consumes it.
    std::expected<std::vector<VkSemaphore>, VkResult> createRenderCompleteSemaphores(VkDevice device,
                                                                                     uint32_t imageCount)
    {
        std::vector<VkSemaphore> semaphores(imageCount, VK_NULL_HANDLE);
        const VkSemaphoreCreateInfo sci{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (uint32_t i = 0; i < imageCount; ++i)
        {
            if (const VkResult r = vkCreateSemaphore(device, &sci, nullptr, &semaphores[i]); r != VK_SUCCESS)
            {
                spdlog::error("[vulkan] vkCreateSemaphore (renderComplete)[{}] failed: {}", i,
                              renderer::vkResultString(r));
                for (VkSemaphore s : semaphores)
                {
                    if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
                }
                return std::unexpected(r);
            }
        }
        return semaphores;
    }

    bool hasLayer(const std::vector<VkLayerProperties>& layers, std::string_view name)
    {
        return std::ranges::any_of(layers, [name](const VkLayerProperties& l)
                                   { return std::string_view(l.layerName) == name; });
    }

    bool hasExtension(const std::vector<VkExtensionProperties>& exts, std::string_view name)
    {
        return std::ranges::any_of(exts, [name](const VkExtensionProperties& e)
                                   { return std::string_view(e.extensionName) == name; });
    }
} // namespace

namespace renderer
{
    // ---- RendererInstance --------------------------------------------------

    RendererInstance::RendererInstance(RendererInstance&& other) noexcept
        : instance_(other.instance_), debugMessenger_(other.debugMessenger_)
    {
        other.instance_ = VK_NULL_HANDLE;
        other.debugMessenger_ = VK_NULL_HANDLE;
    }

    RendererInstance& RendererInstance::operator=(RendererInstance&& other) noexcept
    {
        if (this != &other)
        {
            // Messenger before instance — vkDestroyDebugUtilsMessengerEXT
            // needs the instance live.
            if (debugMessenger_ != VK_NULL_HANDLE) vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
            if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
            instance_ = other.instance_;
            debugMessenger_ = other.debugMessenger_;
            other.instance_ = VK_NULL_HANDLE;
            other.debugMessenger_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    RendererInstance::~RendererInstance()
    {
        if (debugMessenger_ != VK_NULL_HANDLE) vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    }

    std::expected<RendererInstance, VkResult> RendererInstance::create(const std::string& appName,
                                                                       std::span<const char* const> platformExtensions)
    {
        if (VkResult r = volkInitialize(); r != VK_SUCCESS)
        {
            spdlog::error("[vulkan] volkInitialize failed: {}", vkResultString(r));
            return std::unexpected(r);
        }

        auto supportedLayers = getSupportedLayers();
        if (!supportedLayers) return std::unexpected(supportedLayers.error());
        auto supportedExtensions = getSupportedExtensions();
        if (!supportedExtensions) return std::unexpected(supportedExtensions.error());

        std::vector<const char*> extensions(platformExtensions.begin(), platformExtensions.end());
#ifndef NDEBUG
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        for (const char* ext : extensions)
        {
            if (!hasExtension(*supportedExtensions, ext))
            {
                spdlog::error("[vulkan] required instance extension not present: {}", ext);
                return std::unexpected(VK_ERROR_EXTENSION_NOT_PRESENT);
            }
        }

        // Validation in debug only; soft-fail if the SDK's validation layer
        // isn't installed so a fresh dev box still runs.
        std::vector<const char*> layers;
#ifndef NDEBUG
        if (hasLayer(*supportedLayers, "VK_LAYER_KHRONOS_validation"))
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }
#endif

        VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = appName.c_str(),
            .apiVersion = VK_API_VERSION_1_3,
        };

#ifndef NDEBUG
        // Used twice: as pNext on VkInstanceCreateInfo (so the temporary
        // messenger catches messages emitted by vkCreateInstance /
        // vkDestroyInstance themselves) and as the create-info for the
        // persistent messenger below. Severity is warning+error by default —
        // verbose/info from validation are extremely chatty and rarely
        // actionable. Flip bits here to crank up.
        VkDebugUtilsMessengerCreateInfoEXT debugCI{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = vulkanDebugCallback,
        };
#endif

        const VkInstanceCreateInfo ci{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#ifndef NDEBUG
            .pNext = &debugCI,
#endif
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        VkInstance instance = VK_NULL_HANDLE;
        if (VkResult r = vkCreateInstance(&ci, nullptr, &instance); r != VK_SUCCESS)
        {
            spdlog::error("[vulkan] vkCreateInstance failed: {}", vkResultString(r));
            return std::unexpected(r);
        }

        // Resolve instance-level entry points. InstanceOnly (vs LoadInstance)
        // leaves device functions unresolved until volkLoadDevice runs after
        // vkCreateDevice — catches "forgot step 3" bugs at call time.
        volkLoadInstanceOnly(instance);

        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
#ifndef NDEBUG
        if (const VkResult r = vkCreateDebugUtilsMessengerEXT(instance, &debugCI, nullptr, &debugMessenger);
            r != VK_SUCCESS)
        {
            spdlog::error("[vulkan] vkCreateDebugUtilsMessengerEXT failed: {}", vkResultString(r));
            vkDestroyInstance(instance, nullptr);
            return std::unexpected(r);
        }
#endif

        return RendererInstance(instance, debugMessenger);
    }

    std::expected<Renderer, VkResult> RendererInstance::bindSurface(VkSurfaceKHR surface, VkExtent2D extent) &&
    {
        // Physical-device pick needs the surface (queue family must support both
        // graphics and present to *this* surface), so the whole device pipeline
        // lives here in phase 2 rather than in create().

        auto physicalDevice = pickPhysicalDevice(instance_, surface);
        if (!physicalDevice)
        {
            // No 1:1 VkResult for "no suitable device" — the specific reason
            // was logged inside pickPhysicalDevice; surface this as
            // INITIALIZATION_FAILED at the boundary.
            vkDestroySurfaceKHR(instance_, surface, nullptr);
            return std::unexpected(VK_ERROR_INITIALIZATION_FAILED);
        }

        auto logicalDevice = createLogicalDevice(*physicalDevice);
        if (!logicalDevice)
        {
            vkDestroySurfaceKHR(instance_, surface, nullptr);
            return std::unexpected(logicalDevice.error());
        }

        volkLoadDevice(*logicalDevice);

        VkQueue graphicsQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(*logicalDevice, physicalDevice->graphicsQueueIdx, 0, &graphicsQueue);

        const VkInstance instance = instance_;
        const VkDebugUtilsMessengerEXT debugMessenger = debugMessenger_;
        // disarm — the new Renderer owns the instance + messenger now. Do this
        // before constructing the Renderer so that if recreateSwapchain_ fails,
        // its dtor cleans up *all* the bound state (instance included) — the
        // "lose both phases on failure" policy stays whole.
        instance_ = VK_NULL_HANDLE;
        debugMessenger_ = VK_NULL_HANDLE;
        Renderer renderer(instance, debugMessenger, surface, physicalDevice->device, *logicalDevice, graphicsQueue,
                          physicalDevice->graphicsQueueIdx, extent);
        auto frameSync = createFrameSync(*logicalDevice);
        if (!frameSync) return std::unexpected(frameSync.error());
        renderer.fences = frameSync->fences;
        renderer.imageAcquiredSemaphores = frameSync->imageAcquiredSemaphores;
        auto cmd = createCommandResources(*logicalDevice, physicalDevice->graphicsQueueIdx);
        if (!cmd) return std::unexpected(cmd.error());
        renderer.commandPool_ = cmd->pool;
        renderer.commandBuffers_ = cmd->buffers;
        if (auto r = renderer.recreateSwapchain_(extent); !r) return std::unexpected(r.error());
        return renderer;
    }

    // ---- Renderer::recreateSwapchain_ --------------------------------------

    std::expected<void, VkResult> Renderer::recreateSwapchain_(VkExtent2D windowExtent)
    {
        // On recreate (not initial), idle the device so anything referencing
        // the old swapchain's images has finished. Fence-tracking the in-flight
        // frames against the old swapchain is the later optimization; for now
        // a full wait keeps the teardown trivially safe.
        if (swapchain_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);

        VkSurfaceCapabilitiesKHR caps{};
        VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps));

        // Choose the swapchain extent. When the surface dictates a size
        // (currentExtent != UINT32_MAX — Win32, most platforms), trust it.
        // When it doesn't (sentinel — Wayland and some MoltenVK configs), the
        // app picks; clamp the platform-reported window size against the
        // caps' min/max range.
        VkExtent2D chosenExtent;
        if (caps.currentExtent.width != UINT32_MAX)
        {
            chosenExtent = caps.currentExtent;
        }
        else
        {
            chosenExtent.width =
                std::clamp(windowExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
            chosenExtent.height =
                std::clamp(windowExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        // minImageCount + 1 gives one image of slack above the driver's
        // minimum so vkAcquireNextImageKHR doesn't have to block waiting for
        // the presentation engine to release one. Not specifically "triple-
        // buffering" — that's just what it works out to when minImageCount
        // is 2 (typical desktop). Platforms with minImageCount of 3 (Android,
        // some MoltenVK configs) land at 4; the slack property still holds.
        // maxImageCount == 0 means "no upper bound" per spec — hence the guard.
        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

        uint32_t formatCount = 0;
        VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr));
        if (formatCount == 0)
        {
            spdlog::error("[vulkan] surface reports zero supported formats");
            return std::unexpected(VK_ERROR_FORMAT_NOT_SUPPORTED);
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data()));
        // Prefer BGRA8 sRGB so the swapchain does linear->sRGB encoding for us
        // on present; shaders write linear, monitor sees sRGB. Fall back to
        // whatever the surface reports first if that combo isn't available.
        VkSurfaceFormatKHR chosenFormat = formats[0];
        for (const auto& f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosenFormat = f;
                break;
            }
        }

        // FIFO is the only present mode guaranteed available. It's vsync —
        // good default for a game engine; revisit when we want low-latency
        // (MAILBOX) or uncapped (IMMEDIATE) options as a setting.
        constexpr VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

        // Pass the old swapchain so the driver can recycle resources. We
        // destroy it ourselves after the new one is created — vkCreateSwapchainKHR
        // retires the old one but doesn't free it.
        const VkSwapchainKHR oldSwapchain = swapchain_;
        const VkSwapchainCreateInfoKHR sci{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface_,
            .minImageCount = imageCount,
            .imageFormat = chosenFormat.format,
            .imageColorSpace = chosenFormat.colorSpace,
            .imageExtent = chosenExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            // Single graphics+present queue family (pickPhysicalDevice enforces
            // it), so EXCLUSIVE — no concurrent-access fan-out needed.
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform = caps.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = oldSwapchain,
        };
        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        VK_TRY(vkCreateSwapchainKHR(device_, &sci, nullptr, &newSwapchain));

        // New swapchain is live; tear down the old views (they reference
        // soon-to-be-invalid images) and the old per-image renderComplete
        // semaphores (image count may change across recreate), then the old
        // swapchain itself. vkDeviceWaitIdle above guarantees nothing pending
        // is still referencing them.
        for (VkImageView v : swapchainImageViews_)
        {
            if (v != VK_NULL_HANDLE) vkDestroyImageView(device_, v, nullptr);
        }
        swapchainImageViews_.clear();
        for (VkSemaphore s : renderCompleteSemaphores)
        {
            if (s != VK_NULL_HANDLE) vkDestroySemaphore(device_, s, nullptr);
        }
        renderCompleteSemaphores.clear();
        if (oldSwapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);

        swapchain_ = newSwapchain;
        swapchainFormat_ = chosenFormat.format;
        extent_ = chosenExtent;

        uint32_t actualImageCount = 0;
        VK_TRY(vkGetSwapchainImagesKHR(device_, swapchain_, &actualImageCount, nullptr));
        swapchainImages_.resize(actualImageCount);
        VK_TRY(vkGetSwapchainImagesKHR(device_, swapchain_, &actualImageCount, swapchainImages_.data()));

        // Default-construct slots to VK_NULL_HANDLE so a mid-loop failure
        // leaves the vector in a state destroy_() can sweep cleanly.
        swapchainImageViews_.assign(actualImageCount, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < actualImageCount; ++i)
        {
            const VkImageViewCreateInfo ivci{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchainImages_[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapchainFormat_,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            };
            VK_TRY(vkCreateImageView(device_, &ivci, nullptr, &swapchainImageViews_[i]));
        }

        auto sems = createRenderCompleteSemaphores(device_, actualImageCount);
        if (!sems) return std::unexpected(sems.error());
        renderCompleteSemaphores = std::move(*sems);

        spdlog::info("[vulkan] swapchain {}: {}x{}, {} images, format {}",
                     oldSwapchain == VK_NULL_HANDLE ? "created" : "recreated", extent_.width, extent_.height,
                     actualImageCount, static_cast<int>(swapchainFormat_));
        return {};
    }
} // namespace renderer
