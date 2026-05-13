#include "renderer/renderer.h"

#include <algorithm>
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

    enum class DeviceSelectError
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
            typeBonus = 0;
            break;
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

    RendererInstance::RendererInstance(RendererInstance&& other) noexcept : instance_(other.instance_)
    {
        other.instance_ = VK_NULL_HANDLE;
    }

    RendererInstance& RendererInstance::operator=(RendererInstance&& other) noexcept
    {
        if (this != &other)
        {
            if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
            instance_ = other.instance_;
            other.instance_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    RendererInstance::~RendererInstance()
    {
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

        const VkInstanceCreateInfo ci{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
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

        return RendererInstance(instance);
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
        instance_ = VK_NULL_HANDLE; // disarm — Renderer owns the instance now
        return Renderer(instance, surface, physicalDevice->device, *logicalDevice, graphicsQueue,
                        physicalDevice->graphicsQueueIdx, extent);
    }

    // ---- Renderer ----------------------------------------------------------

    void Renderer::destroy_() noexcept
    {
        // Dependency order: device-owned resources -> device -> surface -> instance.
        // (Queue/physical-device handles aren't destroyed — their lifetimes are
        // tied to device and instance respectively.)
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
        device_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
        instance_ = VK_NULL_HANDLE;
        physicalDevice_ = VK_NULL_HANDLE;
        graphicsQueue_ = VK_NULL_HANDLE;
    }

    Renderer::Renderer(Renderer&& other) noexcept
        : instance_(other.instance_), surface_(other.surface_), physicalDevice_(other.physicalDevice_),
          device_(other.device_), graphicsQueue_(other.graphicsQueue_), graphicsQueueIdx_(other.graphicsQueueIdx_),
          extent_(other.extent_)
    {
        other.instance_ = VK_NULL_HANDLE;
        other.surface_ = VK_NULL_HANDLE;
        other.physicalDevice_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.graphicsQueue_ = VK_NULL_HANDLE;
    }

    Renderer& Renderer::operator=(Renderer&& other) noexcept
    {
        if (this != &other)
        {
            destroy_();
            instance_ = other.instance_;
            surface_ = other.surface_;
            physicalDevice_ = other.physicalDevice_;
            device_ = other.device_;
            graphicsQueue_ = other.graphicsQueue_;
            graphicsQueueIdx_ = other.graphicsQueueIdx_;
            extent_ = other.extent_;
            other.instance_ = VK_NULL_HANDLE;
            other.surface_ = VK_NULL_HANDLE;
            other.physicalDevice_ = VK_NULL_HANDLE;
            other.device_ = VK_NULL_HANDLE;
            other.graphicsQueue_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    Renderer::~Renderer()
    {
        destroy_();
    }
} // namespace renderer
